#include "key_decoder.h"
#include "key_normalizer.h"
#include "url_extractor.h"

namespace KeyDecoder
{
    // --- 명령어 파싱 헬퍼 ---

    static void ToLowerInPlace(std::wstring& s)
    {
        for (auto& c : s)
            if (c >= L'A' && c <= L'Z') c = c - L'A' + L'a';
    }

    // &&, ||, |, ; 기준으로 체인 분리
    static std::vector<std::wstring> SplitChain(const std::wstring& s)
    {
        std::vector<std::wstring> parts;
        size_t start = 0;

        for (size_t i = 0; i < s.size(); ) {
            if (i + 1 < s.size() &&
                ((s[i] == L'&' && s[i+1] == L'&') ||
                 (s[i] == L'|' && s[i+1] == L'|'))) {
                parts.push_back(s.substr(start, i - start));
                i += 2; start = i;
                continue;
            }
            if (s[i] == L'|' && (i + 1 >= s.size() || s[i+1] != L'|')) {
                parts.push_back(s.substr(start, i - start));
                i++; start = i;
                continue;
            }
            if (s[i] == L';') {
                parts.push_back(s.substr(start, i - start));
                i++; start = i;
                continue;
            }
            i++;
        }
        parts.push_back(s.substr(start));
        return parts;
    }

    static std::wstring Trim(const std::wstring& s)
    {
        size_t a = s.find_first_not_of(L" \t");
        if (a == std::wstring::npos) return L"";
        size_t b = s.find_last_not_of(L" \t");
        return s.substr(a, b - a + 1);
    }

    static bool IsAlpha(wchar_t c)
    {
        return (c >= L'A' && c <= L'Z') || (c >= L'a' && c <= L'z');
    }

    // 토큰 시작이 . + 알파벳인 경우 . 제거 (메서드/프로퍼티 접근자 정리)
    static void StripLeadingDot(std::wstring& token)
    {
        if (token.size() >= 2 && token[0] == L'.' && IsAlpha(token[1]))
            token.erase(0, 1);
    }

    // 깊이 카운터 기반 짝 맞는 close 찾아서 내용 추출
    // 중첩 괄호 처리, 짝 안 맞으면 false 반환
    static bool ExtractBracketContent(const std::wstring& s,
                                       wchar_t open, wchar_t close,
                                       size_t startIdx,
                                       std::wstring& out)
    {
        if (startIdx >= s.size() || s[startIdx] != open) return false;
        int depth = 1;
        for (size_t i = startIdx + 1; i < s.size(); ++i) {
            if (s[i] == open) depth++;
            else if (s[i] == close && --depth == 0) {
                out = s.substr(startIdx + 1, i - startIdx - 1);
                return true;
            }
        }
        return false;
    }

    static ParsedCommand ParseSegment(const std::wstring& seg, size_t lineIdx)
    {
        ParsedCommand cmd;
        cmd.lineIndex = lineIdx;

        std::vector<std::wstring> tokens;
        std::wstring tok;
        for (wchar_t c : seg) {
            if (c == L' ' || c == L'\t') {
                if (!tok.empty()) {
                    StripLeadingDot(tok);
                    if (!tok.empty()) tokens.push_back(tok);
                    tok.clear();
                }
            } else {
                tok += c;
            }
        }
        if (!tok.empty()) {
            StripLeadingDot(tok);
            if (!tok.empty()) tokens.push_back(tok);
        }
        if (tokens.empty()) return cmd;

        cmd.command = tokens[0];
        ToLowerInPlace(cmd.command);

        for (size_t i = 1; i < tokens.size(); ++i)
            cmd.args.push_back(tokens[i]);

        return cmd;
    }

    // args 안의 (...) {...} $(...) 내용을 추출하여 ParseSegment 재호출.
    // 추출된 ParsedCommand를 result.commands에 추가. 재귀 진입은 깊이 4까지 제한.
    // 예외 처리:
    //   - 따옴표 시작 토큰 skip (괄호가 문자열 일부인 경우)
    //   - 짝 안 맞는 괄호 skip (ExtractBracketContent가 false 반환)
    //   - 중첩 괄호: 깊이 카운터로 짝 맞는 close 찾음
    //   - @(...), @{...}: arg[0]이 '(', '{' 가 아니라 '@' 라 자동 회피
    static void ExtractFromArgs(DecodeResult& result,
                                 size_t scanStart = 0,
                                 int recursionDepth = 0)
    {
        if (recursionDepth > 4) return;

        std::vector<ParsedCommand> extras;
        size_t scanEnd = result.commands.size();

        for (size_t ci = scanStart; ci < scanEnd; ++ci) {
            auto& cmd = result.commands[ci];
            for (auto& arg : cmd.args) {
                if (arg.empty()) continue;
                if (arg[0] == L'"' || arg[0] == L'\'') continue;

                std::wstring inner;
                bool ok = false;

                if (arg[0] == L'(')
                    ok = ExtractBracketContent(arg, L'(', L')', 0, inner);
                else if (arg[0] == L'{')
                    ok = ExtractBracketContent(arg, L'{', L'}', 0, inner);
                else if (arg.size() >= 2 && arg[0] == L'$' && arg[1] == L'(')
                    ok = ExtractBracketContent(arg, L'(', L')', 1, inner);

                if (!ok || inner.empty()) continue;

                for (auto& part : SplitChain(inner)) {
                    auto seg = Trim(part);
                    if (seg.empty()) continue;
                    auto sub = ParseSegment(seg, cmd.lineIndex);
                    if (!sub.command.empty())
                        extras.push_back(std::move(sub));
                }
            }
        }

        if (!extras.empty()) {
            for (auto& e : extras)
                result.commands.push_back(std::move(e));
            // 다음 재귀에서는 새로 추가된 commands만 스캔 (scanEnd 부터)
            ExtractFromArgs(result, scanEnd, recursionDepth + 1);
        }
    }

    static void ParseCommands(DecodeResult& result)
    {
        for (size_t i = 0; i < result.lines.size(); ++i) {
            const std::wstring& text = result.lines[i].text;
            if (text.empty()) continue;

            for (auto& part : SplitChain(text)) {
                auto seg = Trim(part);
                if (seg.empty()) continue;
                auto cmd = ParseSegment(seg, i);
                if (!cmd.command.empty())
                    result.commands.push_back(std::move(cmd));
            }
        }

        // 괄호/중괄호/$() 안의 cmdlet 추출 (방법 E)
        ExtractFromArgs(result);
    }
    DecodeResult Decode(const std::vector<KeyboardHook::KeyRecord>& buf)
    {
        DecodeResult result;

        // --- 1단계: 정규화 (줄 분리 + 단축키 감지) ---
        auto norm = KeyNormalizer::Normalize(buf);

        // 단축키 이벤트 복사
        for (auto& h : norm.hotkeys)
            result.hotkeys.push_back({ h.combo, h.submenuKey, h.lineIndex });

        // 줄 목록 구성 + fullText 조합
        std::wstring fullText;
        for (size_t i = 0; i < norm.lines.size(); ++i) {
            DecodedLine dl;
            dl.text = norm.lines[i];
            // URL 추출은 UrlExtractor 구현 후 여기서 호출 예정
            result.lines.push_back(std::move(dl));

            if (!fullText.empty()) fullText += L"\n";
            fullText += norm.lines[i];
        }
        result.fullText = fullText;

        // --- 2단계: URL 추출 ---
        UrlExtractor::Extract(result.lines);

        // --- 3단계: 명령어 파싱 ---
        ParseCommands(result);

        return result;
    }
}
