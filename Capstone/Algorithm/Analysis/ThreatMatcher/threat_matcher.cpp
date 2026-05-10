#include "threat_matcher.h"
#include "threat_db_connect.h"
#include <algorithm>
#include <cmath>
#include <iomanip>
#include <regex>
#include <set>
#include <sstream>
#include <utility>

#include <Windows.h>
#include <ShlObj.h>
#include <wincrypt.h>     // Phase 5: base64 디코드 (CryptStringToBinaryW)
#pragma comment(lib, "crypt32.lib")

// ──────────────────────────────────────────────────────────────────────
// ThreatMatcher 본체
//
// 책임:
//   - args 토큰화 (param/value 페어 분리)
//   - per-command + per-token 매칭 오케스트레이션
//   - 세션 통계 집계 (axis 카운트, 활성 태그, session_score)
//   - 검증/디버깅 로그 포맷팅
//
// DB 연결과 lookup 은 tm_db 모듈에서 담당 (분리).
// ──────────────────────────────────────────────────────────────────────

namespace ThreatMatcher
{
    namespace {

        // ─── 토큰 헬퍼 ─────────────────────────────────────────────
        bool IsFlag(const std::wstring& s)
        {
            if (s.empty()) return false;
            wchar_t c = s[0];
            return c == L'-' || c == L'/' || c == L'+';
        }

        // args → (param, value) 페어 리스트.
        //   1) flag 토큰 안에 ':' 가 있으면 PS 의 -Param:Value 문법 → 그 자리에서 split
        //      (예: "-Confirm:$false" → param="-Confirm", value="$false")
        //   2) param 다음이 또 flag → param 만 (value='')
        //   3) param 다음이 일반 토큰 → 짝짓기
        //   4) 단독 일반 토큰 → (param='', value=token) 위치 인자
        std::vector<std::pair<std::wstring, std::wstring>>
        SplitParamValuePairs(const std::vector<std::wstring>& args)
        {
            std::vector<std::pair<std::wstring, std::wstring>> out;
            out.reserve(args.size());
            for (size_t i = 0; i < args.size(); ) {
                const auto& a = args[i];
                if (IsFlag(a)) {
                    // PS '-Param:Value' 문법 처리:
                    //   첫 ':' 위치 (단, 위치 > 1 이어야 — '-:value' 같은 건 제외)
                    auto colon = a.find(L':');
                    if (colon != std::wstring::npos && colon > 1) {
                        std::wstring p = a.substr(0, colon);
                        std::wstring v = a.substr(colon + 1);
                        out.emplace_back(p, v);
                        i += 1;
                        continue;
                    }
                    if (i + 1 < args.size() && !IsFlag(args[i + 1])) {
                        out.emplace_back(a, args[i + 1]);
                        i += 2;
                    } else {
                        out.emplace_back(a, std::wstring{});
                        i += 1;
                    }
                } else {
                    out.emplace_back(std::wstring{}, a);
                    i += 1;
                }
            }
            return out;
        }

        // ─── 토큰 1개 분석 (lookup 우선순위 적용) ──────────────────
        TokenMatch AnalyzeToken(int command_id,
                                 const std::wstring& cmd_name,
                                 const std::wstring& param,
                                 const std::wstring& value)
        {
            TokenMatch tk{};
            tk.param = param;
            tk.value = value;
            tk.axis  = L'X';

            // 1) common_parameters (글로벌 PowerShell 공통 매개변수)
            if (!param.empty()) {
                auto r = DB::LookupCommonParameter(param);
                if (r.found) {
                    tk.tag_key     = r.tag_key;
                    tk.axis        = r.axis;
                    tk.base_score  = r.base_score;
                    tk.from_common = true;
                    tk.in_db       = true;
                    tk.matched     = (r.axis != L'X');
                }
            }

            // 2) parameters (cmdlet 특정) — common 매칭 못한 경우만
            if (!tk.matched && !param.empty() && command_id > 0) {
                auto r = DB::LookupParameter(command_id, param);
                if (r.found) {
                    tk.tag_key    = r.tag_key;
                    tk.axis       = r.axis;
                    tk.base_score = r.base_score;
                    tk.in_db      = true;
                    tk.matched    = (r.axis != L'X');
                }
            }

            // 3) enum_threats — value 가 있을 때만 가산
            //   값이 메서드 체이닝 형태일 수 있음 (예: `Shell.Application).MinimizeAll()`).
            //   첫 ')' 또는 '}' 이전까지를 진짜 값으로 간주하고 lookup.
            if (!value.empty()) {
                std::wstring lookup_value = value;
                auto cut = lookup_value.find_first_of(L")}");
                if (cut != std::wstring::npos)
                    lookup_value = lookup_value.substr(0, cut);
                if (!lookup_value.empty())
                    tk.enum_adjust = DB::LookupEnumAdjust(cmd_name, param, lookup_value);
            }

            // 4) 토큰 점수 (음수 floor 0)
            tk.token_score = (std::max)(0.0, tk.base_score + tk.enum_adjust);
            return tk;
        }

        // ─── 명령어 1개 분석 ──────────────────────────────────────
        CommandMatch AnalyzeCommand(const KeyDecoder::ParsedCommand& parsed,
                                     int shell_id)
        {
            CommandMatch cm{};
            cm.command   = parsed.command;
            cm.lineIndex = parsed.lineIndex;
            cm.phase     = 1;        // KeyDecoder 가 직접 추출한 라인

            // multi-word lookup 시도 (reg add, sc query 등)
            //   args[0] 이 flag 가 아니면 합쳐서 재시도 → 매칭 시 args[0] 소비
            std::wstring next_token = parsed.args.empty() ? L"" : parsed.args[0];
            bool used_next = false;
            int cid = DB::LookupCommand(parsed.command, shell_id, next_token, &used_next);
            cm.in_db = (cid > 0);
            if (cid <= 0) return cm;

            // multi-word 매칭 시 effective args 는 args[1..]
            const std::vector<std::wstring>& eff_args =
                used_next ? std::vector<std::wstring>(parsed.args.begin() + 1, parsed.args.end())
                          : parsed.args;
            if (used_next) {
                cm.command = parsed.command + L" " + parsed.args[0];  // 표시용
            }

            // command-self fallback 헬퍼 (인자 0개 또는 매칭 0개 케이스)
            auto applyCommandSelf = [&]() {
                for (const auto& r : DB::LookupCommandSelf(cid)) {
                    TokenMatch tk{};
                    tk.tag_key     = r.tag_key;
                    tk.axis        = r.axis;
                    tk.base_score  = r.base_score;
                    tk.token_score = r.base_score;
                    tk.matched     = (r.axis != L'X');
                    cm.tokens.push_back(std::move(tk));
                    cm.cmd_raw += r.base_score;
                }
            };

            // 케이스 1: effective args 0개 → command-self
            if (eff_args.empty()) {
                applyCommandSelf();
                return cm;
            }

            // (param, value) 페어 단위로 분석
            auto pairs = SplitParamValuePairs(eff_args);
            int recognized_pairs = 0;  // DB 에 있는 인자 수 (axis 무관)
            for (auto& pv : pairs) {
                TokenMatch tk = AnalyzeToken(cid, cm.command, pv.first, pv.second);
                if (tk.in_db) ++recognized_pairs;
                cm.cmd_raw += tk.token_score;
                cm.tokens.push_back(std::move(tk));
            }

            // 케이스 2: 인자 있지만 모두 DB 미인식 → cmd 의 본질적 역할로 보완
            //   (사용자가 오타하거나 알 수 없는 옵션 사용한 경우)
            //   axis='X' 로 DB 에 등록된 인자 (= MD 가 의도적으로 미매칭 분류) 는
            //   "인식됨" 으로 간주 → fallback 안 함 (MD 의도 존중)
            if (recognized_pairs == 0) {
                applyCommandSelf();
            }
            return cm;
        }

        // ─── Phase 4: fullText 정규식 보완 스캔 (SPEC §5.5) ─────
        //   Phase 1 (commands[]) 에서 못 잡은 cmdlet 을 fullText 에서
        //   PowerShell Verb-Noun 패턴 정규식으로 추출 후 보완 매칭.
        //   적용 대상: 따옴표 안 매몰 cmdlet, 제어구조 안 일부 등.
        //   한계: args 정보 없음 → command-self / base 점수만.
        void Phase4FullTextScan(const KeyDecoder::DecodeResult& decoded,
                                 int shell_id,
                                 ThreatResult& result)
        {
            if (decoded.fullText.empty()) return;

            // 이미 매칭된 cmd 이름 집합 (Phase 1 에서 처리된 것들)
            // canonical command_id set (Phase 1 cm.command may have prefix like "(new-object")
            std::set<int> already_ids;
            for (const auto& cm : result.commands) {
                int cid = DB::LookupCommand(cm.command, shell_id);
                if (cid > 0) already_ids.insert(cid);
            }

            // PowerShell 표준 Verb-Noun 정규식 (SPEC §5.5 그대로)
            std::wregex pat(LR"(\b([A-Z][a-zA-Z]+)-([A-Z][a-zA-Z]+)\b)");
            auto begin = std::wsregex_iterator(decoded.fullText.begin(),
                                                decoded.fullText.end(), pat);
            auto end   = std::wsregex_iterator();

            for (auto it = begin; it != end; ++it) {
                std::wstring name = it->str();
                // lowercase
                for (auto& c : name)
                    if (c >= L'A' && c <= L'Z') c = c - L'A' + L'a';

                int cid = DB::LookupCommand(name, shell_id);
                if (cid <= 0) continue;
                if (already_ids.count(cid)) continue;  // canonical id duplicate
                already_ids.insert(cid);

                // args 정보 없음 → command-self row 들로 점수 부여
                CommandMatch cm{};
                cm.command   = name;
                cm.lineIndex = (size_t)-1;  // line 추적 안 됨
                cm.in_db     = true;
                cm.phase     = 4;          // fullText 정규식 보완 결과

                for (const auto& r : DB::LookupCommandSelf(cid)) {
                    TokenMatch tk{};
                    tk.tag_key     = r.tag_key;
                    tk.axis        = r.axis;
                    tk.base_score  = r.base_score;
                    tk.token_score = r.base_score;
                    tk.matched     = (r.axis != L'X');
                    cm.tokens.push_back(std::move(tk));
                    cm.cmd_raw += r.base_score;
                }

                result.commands.push_back(std::move(cm));
            }
        }

        // ═══════════════════════════════════════════════════════════════
        // Phase 5: 인용문 안 매몰된 스크립트 unwrap → 재분석
        // ═══════════════════════════════════════════════════════════════
        //
        // 트리거 패턴 (= "executor" 명령) 식별 후, 인용문 안 코드를 끌어내어
        // 다시 ThreatMatcher 에 입력하는 사후처리.
        //
        // 잡는 케이스 (3가지):
        //   1.  cmd /c "X & Y & Z"            → 내부 = cmd shell 명령들
        //   2.  cmd /k "X & Y"                → 동일
        //   3.  powershell -Command "X; Y"   → 내부 = PowerShell 코드
        //   4.  powershell -c "X; Y"         → 동일 (-c 는 -Command alias)
        //   5.  powershell -EncodedCommand <base64>   → base64 디코드 후 PS 코드
        //   6.  powershell -e <base64> / -enc <base64>  → 동일
        //
        // 처리 흐름:
        //   ① result.commands 순회 → executor cmd 검출
        //   ② 인용문 strip + (필요시) base64 디코드
        //   ③ 체인 분리자로 split:
        //        cmd shell : & && || | ; \n
        //        powershell: ; | \n        (& 은 PS 에서 background → 분기 X)
        //   ④ 각 segment 토큰화 → ParsedCommand → AnalyzeCommand 재호출
        //   ⑤ 결과를 result.commands 에 phase=5 로 추가
        //   ⑥ 재귀 — 새로 추가된 cmd 안에 또 인용문 있으면 다시 (depth ≤ 2)
        //
        // Edge cases / 한계:
        //   - 중첩 인용문 escape (\")
        //   - PowerShell here-string (@"..."@) — 미지원
        //   - base64 디코드 실패 → 해당 cmd skip
        //   - 빈 segment 무시
        //   - 변수 할당 ($var=cmd) — 첫 토큰만 cmd 로 추출되므로 미인식 (한계)

        namespace {
            // 양쪽 따옴표 strip + 안의 escape 해석
            //   "X \"Y\" Z" → X "Y" Z
            //   양쪽 따옴표 없으면 그대로 반환
            std::wstring StripOuterQuotes(const std::wstring& s)
            {
                if (s.size() < 2) return s;
                wchar_t q = s.front();
                if ((q != L'"' && q != L'\'') || s.back() != q) return s;
                std::wstring inner = s.substr(1, s.size() - 2);
                // escape 해석: \" 는 " 로, \\ 는 \ 로 변환 (백슬래시 끝맺음 주의)
                std::wstring out;
                out.reserve(inner.size());
                for (size_t i = 0; i < inner.size(); ++i) {
                    if (inner[i] == L'\\' && i + 1 < inner.size()) {
                        wchar_t nx = inner[i + 1];
                        if (nx == L'"' || nx == L'\'' || nx == L'\\') {
                            out.push_back(nx); ++i; continue;
                        }
                    }
                    out.push_back(inner[i]);
                }
                return out;
            }

            // base64 → UTF-16 LE wstring
            //   PowerShell -EncodedCommand 는 UTF-16 LE 로 인코딩된 PS 코드를 base64
            bool TryBase64DecodeUtf16(const std::wstring& b64, std::wstring& out)
            {
                DWORD byte_count = 0;
                if (!CryptStringToBinaryW(b64.c_str(), 0, CRYPT_STRING_BASE64,
                                           nullptr, &byte_count, nullptr, nullptr))
                    return false;
                if (byte_count == 0 || (byte_count % 2) != 0) return false;
                std::vector<BYTE> bytes(byte_count);
                if (!CryptStringToBinaryW(b64.c_str(), 0, CRYPT_STRING_BASE64,
                                           bytes.data(), &byte_count, nullptr, nullptr))
                    return false;
                out.assign(reinterpret_cast<wchar_t*>(bytes.data()), byte_count / 2);
                return true;
            }

            // 한 명령의 (param, value) 들을 보고 executor 패턴 검출
            //   반환 시: out_inner = 인용문 strip & decode 끝낸 실제 코드
            //          out_sub_shell = 1(CMD) / 2(POWERSHELL)
            struct ExecutorMatch {
                std::wstring inner;     // 분석 대상 내부 코드
                int          sub_shell; // 1=CMD / 2=POWERSHELL
            };

            bool DetectExecutor(const CommandMatch& cm, ExecutorMatch& out)
            {
                std::wstring c = cm.command;
                // lowercase
                for (auto& ch : c) if (ch >= L'A' && ch <= L'Z') ch = ch - L'A' + L'a';

                bool is_cmd = (c == L"cmd" || c == L"cmd.exe");
                bool is_ps  = (c == L"powershell" || c == L"powershell.exe"
                            || c == L"pwsh"       || c == L"pwsh.exe");
                if (!is_cmd && !is_ps) return false;

                // tokens 안에서 트리거 param 찾기
                for (size_t i = 0; i < cm.tokens.size(); ++i) {
                    std::wstring p = cm.tokens[i].param;
                    for (auto& ch : p) if (ch >= L'A' && ch <= L'Z') ch = ch - L'A' + L'a';
                    std::wstring v = cm.tokens[i].value;

                    if (is_cmd) {
                        // cmd /c "..." or /k "..."
                        if (p == L"/c" || p == L"/k") {
                            out.inner     = StripOuterQuotes(v);
                            out.sub_shell = 1;  // CMD
                            return !out.inner.empty();
                        }
                    }
                    if (is_ps) {
                        // -Command "..." or -c "..."
                        if (p == L"-command" || p == L"-c") {
                            out.inner     = StripOuterQuotes(v);
                            out.sub_shell = 2;  // POWERSHELL
                            return !out.inner.empty();
                        }
                        // -EncodedCommand <base64> or -e / -enc
                        if (p == L"-encodedcommand" || p == L"-e" || p == L"-enc") {
                            std::wstring decoded;
                            if (!TryBase64DecodeUtf16(StripOuterQuotes(v), decoded))
                                return false;
                            out.inner     = decoded;
                            out.sub_shell = 2;
                            return !out.inner.empty();
                        }
                    }
                }
                return false;
            }

            // 체인 분리자로 split — quote-aware (인용문 안 분리자는 무시)
            //   shell=1 (cmd):  & && || | ;  \n
            //   shell=2 (PS):   ; |  \n        (& 은 PS background)
            std::vector<std::wstring> SplitChain(const std::wstring& script, int shell)
            {
                std::vector<std::wstring> out;
                std::wstring cur;
                bool in_q = false;
                wchar_t q = 0;
                for (size_t i = 0; i < script.size(); ++i) {
                    wchar_t c = script[i];
                    if (in_q) {
                        cur.push_back(c);
                        if (c == q) in_q = false;
                        continue;
                    }
                    if (c == L'"' || c == L'\'') {
                        in_q = true; q = c; cur.push_back(c); continue;
                    }
                    bool is_sep = false;
                    if (c == L'\n' || c == L';') is_sep = true;
                    else if (c == L'|') is_sep = true;  // 양 shell 모두 |
                    else if (shell == 1 && c == L'&') is_sep = true;  // CMD only

                    if (is_sep) {
                        // && / || / | / & → 다음 같은 문자도 같이 소비
                        while (i + 1 < script.size() && script[i + 1] == c) ++i;
                        if (!cur.empty()) {
                            std::wstring t = cur;
                            // trim
                            size_t a = t.find_first_not_of(L" \t\r");
                            size_t b = t.find_last_not_of(L" \t\r");
                            if (a != std::wstring::npos)
                                out.push_back(t.substr(a, b - a + 1));
                        }
                        cur.clear();
                    } else {
                        cur.push_back(c);
                    }
                }
                if (!cur.empty()) {
                    size_t a = cur.find_first_not_of(L" \t\r");
                    size_t b = cur.find_last_not_of(L" \t\r");
                    if (a != std::wstring::npos)
                        out.push_back(cur.substr(a, b - a + 1));
                }
                return out;
            }

            // 한 segment ("foo bar baz") → ParsedCommand
            //   첫 토큰 = command, 나머지 = args. quote-aware split.
            KeyDecoder::ParsedCommand SegmentToParsed(const std::wstring& seg)
            {
                KeyDecoder::ParsedCommand p{};
                p.lineIndex = (size_t)-1;
                std::vector<std::wstring> toks;
                std::wstring cur;
                bool in_q = false;
                wchar_t q = 0;
                for (wchar_t c : seg) {
                    if (in_q) {
                        cur.push_back(c);
                        if (c == q) in_q = false;
                    } else if (c == L'"' || c == L'\'') {
                        in_q = true; q = c; cur.push_back(c);
                    } else if (c == L' ' || c == L'\t') {
                        if (!cur.empty()) { toks.push_back(cur); cur.clear(); }
                    } else cur.push_back(c);
                }
                if (!cur.empty()) toks.push_back(cur);
                if (toks.empty()) return p;
                // command 는 lowercase 로
                std::wstring cmd_lc = toks[0];
                for (auto& ch : cmd_lc)
                    if (ch >= L'A' && ch <= L'Z') ch = ch - L'A' + L'a';
                p.command = cmd_lc;
                p.args.assign(toks.begin() + 1, toks.end());
                return p;
            }
        } // anon for Phase 5 helpers

        // scan_start: 검사할 result.commands 의 시작 index.
        //   재귀 호출 시 0 부터 다시 보면 이전 단계에서 이미 처리한 executor 를
        //   또 처리해서 동일 cmd 가 중복 추가됨. 새로 push 된 부분만 봐야 함.
        void Phase5UnquoteAndReanalyze(int shell_id, ThreatResult& result,
                                        int depth, size_t scan_start = 0)
        {
            // 무한 재귀 방지: 깊이 2 까지만 (cmd /c "powershell -c \"...\"" 같은 2단 처리)
            if (depth >= 2) return;

            std::vector<CommandMatch> extras;
            const size_t scan_end = result.commands.size();
            for (size_t i = scan_start; i < scan_end; ++i) {
                const auto& cm = result.commands[i];
                ExecutorMatch em;
                if (!DetectExecutor(cm, em)) continue;

                // 체인 분리 → 각 segment 분석
                auto segs = SplitChain(em.inner, em.sub_shell);
                for (auto& seg : segs) {
                    auto parsed = SegmentToParsed(seg);
                    if (parsed.command.empty()) continue;

                    CommandMatch sub_cm = AnalyzeCommand(parsed, em.sub_shell);
                    sub_cm.lineIndex = (size_t)-1;  // 인용문 출처 — 라인 추적 X
                    sub_cm.phase     = 5;           // FormatLog 가 "line=quoted" 로 표시
                    extras.push_back(std::move(sub_cm));
                }
            }

            if (extras.empty()) return;
            const size_t new_start = result.commands.size();  // 새 entries 시작 index
            for (auto& c : extras) result.commands.push_back(std::move(c));

            // 재귀 — 새로 추가된 것들 [new_start, ...) 만 다시 검사
            Phase5UnquoteAndReanalyze(shell_id, result, depth + 1, new_start);
        }

        // ─── 세션 통계 집계 + session_score 산출 ──────────────────
        void AggregateSession(ThreatResult& result)
        {
            for (const auto& cm : result.commands) {
                for (const auto& tk : cm.tokens) {
                    ++result.total_tokens;
                    if (tk.matched && tk.axis != L'X') {
                        ++result.matched_tokens;
                        result.activated_tags.insert(tk.tag_key);
                        switch (tk.axis) {
                        case L'S': ++result.axis_count_s; break;
                        case L'C': ++result.axis_count_c; break;
                        case L'I': ++result.axis_count_i; break;
                        case L'A': ++result.axis_count_a; break;
                        default: break;
                        }
                    } else {
                        ++result.unmatched_tokens;
                    }
                }
                result.session_raw += cm.cmd_raw;
            }
            const size_t n = result.activated_tags.size();
            if (n > 0) {
                double sq = std::sqrt((double)n);
                result.session_score = (std::min)(10.0, result.session_raw / sq);
            } else {
                result.session_score = 0.0;
            }
        }

        // ─── 로그 포맷팅 헬퍼 ─────────────────────────────────────
        // 토큰 표기: param + " " + value (둘 다 비면 빈 문자열)
        std::wstring TokenLabel(const TokenMatch& tk)
        {
            if (tk.param.empty() && tk.value.empty()) return L"(command-self)";
            if (tk.param.empty()) return L"<positional> " + tk.value;
            if (tk.value.empty()) return tk.param;
            return tk.param + L" " + tk.value;
        }

        // 토큰 1개를 2줄로 출력 (좁은 너비 대응)
        //   ◦ -ExecutionPolicy Bypass
        //     [Remote-Targeting · S] 4.75 + 3.00 = 7.75
        std::wstring FormatToken(const TokenMatch& tk)
        {
            std::wostringstream o;
            o << std::fixed << std::setprecision(2);

            const std::wstring tag = tk.tag_key.empty()
                ? L"<unmatched>"
                : tk.tag_key + L" · " + std::wstring(1, tk.axis)
                  + (tk.from_common ? L" 공통" : L"");

            o << L"  ◦ " << TokenLabel(tk) << L"\r\n"
              << L"    [" << tag << L"] "
              << tk.base_score
              << (tk.enum_adjust >= 0 ? L" + " : L" - ")
              << std::abs(tk.enum_adjust)
              << L" = " << tk.token_score << L"\r\n";
            return o.str();
        }

    } // anon

    // ─── 공개 API ─────────────────────────────────────────────────
    void SetDatabasePath(const std::wstring& path) { DB::SetPath(path); }
    void Shutdown() { DB::Close(); }

    ThreatResult Analyze(const KeyDecoder::DecodeResult& decoded, int shell_id)
    {
        ThreatResult result{};
        if (!DB::Open()) return result;  // DB 못 열면 빈 결과

        // Phase 1~3: 명령어별 (param, value) 매칭
        result.commands.reserve(decoded.commands.size());
        for (const auto& parsed : decoded.commands)
            result.commands.push_back(AnalyzeCommand(parsed, shell_id));

        // Phase 4 (SPEC §5.5): fullText 정규식 보완 — 따옴표 안 / 제어구조 안 cmdlet
        Phase4FullTextScan(decoded, shell_id, result);

        // Phase 5: cmd /c "..." / powershell -Command "..." 안 매몰된 코드 unwrap
        //          + base64 -EncodedCommand 디코드. 재귀 깊이 2 까지.
        Phase5UnquoteAndReanalyze(shell_id, result, 0);

        // 세션 통계 + session_score
        AggregateSession(result);
        return result;
    }

    std::wstring FormatLog(const ThreatResult& result)
    {
        std::wostringstream o;
        o << std::fixed << std::setprecision(2);
        const std::wstring DBL_SEP(40, L'=');
        const std::wstring SEC_SEP(28, L'-');

        // ─── 헤더 ─────────────────────────────────────────────
        o << DBL_SEP << L"\r\n"
          << L" ThreatMatcher 분석\n"
          << DBL_SEP << L"\r\n";

        if (result.commands.empty())
            o << L"\n (분석 대상 없음)\n";

        // ─── 명령어별 ─────────────────────────────────────────
        size_t cmd_idx = 0;
        for (const auto& cm : result.commands) {
            ++cmd_idx;
            o << L"\r\n[" << cmd_idx << L"] ";
            // phase 별 표시:
            //   1 : line=N      (KeyDecoder 가 line 별 추출)
            //   4 : line=fullText  (Phase 4 정규식 보완)
            //   5 : line=quoted    (Phase 5 인용문 unwrap)
            if (cm.phase == 5)
                o << L"line=quoted";
            else if (cm.lineIndex == (size_t)-1)
                o << L"line=fullText";
            else
                o << L"line=" << cm.lineIndex;
            o << L"  " << cm.command
              << (cm.in_db ? L"  ✓" : L"  ✗") << L"\r\n";

            if (cm.tokens.empty()) {
                o << L"  · 토큰 없음\r\n";
                continue;
            }

            for (const auto& tk : cm.tokens)
                o << FormatToken(tk);

            o << L"  " << SEC_SEP << L"\r\n"
              << L"  cmd_raw = "
              << std::fixed << std::setprecision(2) << cm.cmd_raw << L"\r\n";
        }

        // ─── 세션 합산 ─────────────────────────────────────────
        o << L"\r\n" << DBL_SEP << L"\r\n"
          << L" 세션 합산\n"
          << DBL_SEP << L"\r\n"

          << L"\n 토큰 통계\n"
          << L"  전체   : " << result.total_tokens     << L"\r\n"
          << L"  매칭   : " << result.matched_tokens   << L"\r\n"
          << L"  미매칭 : " << result.unmatched_tokens << L"\r\n"

          << L"\n 활성 카테고리 (" << result.activated_tags.size() << L"개)\n";
        if (result.activated_tags.empty()) {
            o << L"  (없음)\n";
        } else {
            for (const auto& t : result.activated_tags)
                o << L"  • " << t << L"\r\n";
        }

        o << L"\n SCIA 축 분포\n"
          << L"  S=" << result.axis_count_s
          << L"  C=" << result.axis_count_c
          << L"  I=" << result.axis_count_i
          << L"  A=" << result.axis_count_a << L"\r\n"

          << L"\n 점수 산출\n"
          << L"  raw 합 = "
          << std::fixed << std::setprecision(2) << result.session_raw << L"\r\n";

        const size_t n = result.activated_tags.size();
        if (n > 0) {
            const double sq = std::sqrt((double)n);
            o << L"  N    = " << n << L"\r\n"
              << L"  √N   = "
              << std::fixed << std::setprecision(3) << sq << L"\r\n"
              << L"  " << SEC_SEP << L"\r\n"
              << L"  session_score = "
              << std::fixed << std::setprecision(3) << result.session_score
              << L"\r\n";
        } else {
            o << L"  session_score = "
              << std::fixed << std::setprecision(3) << result.session_score
              << L"  (활성 카테고리 없음)\n";
        }

        o << DBL_SEP << L"\r\n";
        return o.str();
    }
}
