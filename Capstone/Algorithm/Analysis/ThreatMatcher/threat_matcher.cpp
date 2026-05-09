#include "threat_matcher.h"
#include "threat_db_connect.h"
#include <algorithm>
#include <cmath>
#include <iomanip>
#include <sstream>
#include <utility>

#include <Windows.h>
#include <ShlObj.h>

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
        //   param 다음이 또 flag → param 만 (value='')
        //   param 다음이 일반 토큰 → 짝짓기
        //   단독 일반 토큰 → (param='', value=token) 위치 인자
        std::vector<std::pair<std::wstring, std::wstring>>
        SplitParamValuePairs(const std::vector<std::wstring>& args)
        {
            std::vector<std::pair<std::wstring, std::wstring>> out;
            out.reserve(args.size());
            for (size_t i = 0; i < args.size(); ) {
                const auto& a = args[i];
                if (IsFlag(a)) {
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
                    tk.matched    = (r.axis != L'X');
                }
            }

            // 3) enum_threats — value 가 있을 때만 가산
            if (!value.empty())
                tk.enum_adjust = DB::LookupEnumAdjust(cmd_name, param, value);

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

            int cid = DB::LookupCommand(parsed.command, shell_id);
            cm.in_db = (cid > 0);
            if (cid <= 0) return cm;

            // 인자 비어있으면 command-self row 들로 fallback
            if (parsed.args.empty()) {
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
                return cm;
            }

            // (param, value) 페어 단위로 분석 
            auto pairs = SplitParamValuePairs(parsed.args);
            for (auto& pv : pairs) {
                TokenMatch tk = AnalyzeToken(cid, parsed.command, pv.first, pv.second);
                cm.cmd_raw += tk.token_score;
                cm.tokens.push_back(std::move(tk));
            }
            return cm;
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
        std::wstring FormatToken(const TokenMatch& tk)
        {
            std::wostringstream o;
            o << std::fixed << std::setprecision(3)
              << L"  " << std::left << std::setw(22)
              << (L"'" + tk.param + L"'")
              << L"val='" << tk.value << L"'"
              << L"  → " << (tk.tag_key.empty() ? L"<unmatched>" : tk.tag_key)
              << L" (" << tk.axis << L")"
              << (tk.from_common ? L" [common]" : L"")
              << L"  base=" << tk.base_score
              << L" enum=" << (tk.enum_adjust >= 0 ? L"+" : L"") << tk.enum_adjust
              << L" → " << tk.token_score;
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

        result.commands.reserve(decoded.commands.size());
        for (const auto& parsed : decoded.commands)
            result.commands.push_back(AnalyzeCommand(parsed, shell_id));

        AggregateSession(result);
        return result;
    }

    std::wstring FormatLog(const ThreatResult& result)
    {
        std::wostringstream o;
        o << std::fixed << std::setprecision(3);
        const std::wstring sep(60, L'=');

        o << sep << L"\n"
          << L"ThreatMatcher Analysis\n"
          << sep << L"\n";

        for (const auto& cm : result.commands) {
            o << L"\n[line " << cm.lineIndex << L"] " << cm.command
              << (cm.in_db ? L"  ✓" : L"  ✗ (cmd not in DB)") << L"\n";
            if (cm.tokens.empty()) {
                o << L"  (no tokens)\n";
                continue;
            }
            for (const auto& tk : cm.tokens)
                o << FormatToken(tk) << L"\n";
            o << L"  cmd_raw = " << cm.cmd_raw << L"\n";
        }

        o << L"\n" << std::wstring(60, L'-') << L"\n"
          << L"Session Summary\n"
          << std::wstring(60, L'-') << L"\n"
          << L"  Total tokens:      " << result.total_tokens     << L"\n"
          << L"  Matched tokens:    " << result.matched_tokens   << L"\n"
          << L"  Unmatched tokens:  " << result.unmatched_tokens << L"\n"
          << L"  Activated tags ("  << result.activated_tags.size() << L"):\n";
        for (const auto& t : result.activated_tags)
            o << L"    - " << t << L"\n";
        o << L"  SCIA axis counts:  S=" << result.axis_count_s
          << L", C=" << result.axis_count_c
          << L", I=" << result.axis_count_i
          << L", A=" << result.axis_count_a << L"\n"
          << L"  session_raw:       " << result.session_raw << L"\n";

        size_t n = result.activated_tags.size();
        if (n > 0) {
            double sq = std::sqrt((double)n);
            o << L"  N_categories:      " << n << L", √N = " << sq << L"\n"
              << L"  session_score:     min(10, " << result.session_raw
              << L" / " << sq << L") = " << result.session_score << L"\n";
        } else {
            o << L"  session_score:     " << result.session_score
              << L" (no activated tags)\n";
        }
        return o.str();
    }
}
