#include "threat_matcher_internal.h"

#include <Windows.h>
#include <wincrypt.h>

#include <vector>

#pragma comment(lib, "crypt32.lib")

namespace ThreatMatcher::Internal
{
    namespace
    {
        std::wstring StripOuterQuotes(const std::wstring& s)
        {
            if (s.size() < 2) return s;
            wchar_t q = s.front();
            if ((q != L'"' && q != L'\'') || s.back() != q) return s;

            std::wstring inner = s.substr(1, s.size() - 2);
            std::wstring out;
            out.reserve(inner.size());
            for (size_t i = 0; i < inner.size(); ++i) {
                if (inner[i] == L'\\' && i + 1 < inner.size()) {
                    wchar_t nx = inner[i + 1];
                    if (nx == L'"' || nx == L'\'' || nx == L'\\') {
                        out.push_back(nx);
                        ++i;
                        continue;
                    }
                }
                out.push_back(inner[i]);
            }
            return out;
        }

        bool TryBase64DecodeUtf16(const std::wstring& b64, std::wstring& out)
        {
            DWORD byte_count = 0;
            if (!CryptStringToBinaryW(
                b64.c_str(), 0, CRYPT_STRING_BASE64, nullptr, &byte_count, nullptr, nullptr)) {
                return false;
            }

            if (byte_count == 0 || (byte_count % 2) != 0) {
                return false;
            }

            std::vector<BYTE> bytes(byte_count);
            if (!CryptStringToBinaryW(
                b64.c_str(), 0, CRYPT_STRING_BASE64, bytes.data(), &byte_count, nullptr, nullptr)) {
                return false;
            }

            out.assign(reinterpret_cast<wchar_t*>(bytes.data()), byte_count / 2);
            return true;
        }

        struct ExecutorMatch
        {
            std::wstring inner;
            int sub_shell;
        };

        bool DetectExecutor(const CommandMatch& cm, ExecutorMatch& out)
        {
            std::wstring c = cm.command;
            for (auto& ch : c) {
                if (ch >= L'A' && ch <= L'Z') {
                    ch = ch - L'A' + L'a';
                }
            }

            bool is_cmd = (c == L"cmd" || c == L"cmd.exe");
            bool is_ps =
                (c == L"powershell" || c == L"powershell.exe" ||
                 c == L"pwsh" || c == L"pwsh.exe");
            if (!is_cmd && !is_ps) {
                return false;
            }

            for (const auto& token : cm.tokens) {
                std::wstring p = token.param;
                for (auto& ch : p) {
                    if (ch >= L'A' && ch <= L'Z') {
                        ch = ch - L'A' + L'a';
                    }
                }

                if (is_cmd && (p == L"/c" || p == L"/k")) {
                    out.inner = StripOuterQuotes(token.value);
                    out.sub_shell = 1;
                    return !out.inner.empty();
                }

                if (is_ps && (p == L"-command" || p == L"-c")) {
                    out.inner = StripOuterQuotes(token.value);
                    out.sub_shell = 2;
                    return !out.inner.empty();
                }

                if (is_ps && (p == L"-encodedcommand" || p == L"-e" || p == L"-enc")) {
                    std::wstring decoded;
                    if (!TryBase64DecodeUtf16(StripOuterQuotes(token.value), decoded)) {
                        return false;
                    }
                    out.inner = decoded;
                    out.sub_shell = 2;
                    return !out.inner.empty();
                }
            }

            return false;
        }

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
                    in_q = true;
                    q = c;
                    cur.push_back(c);
                    continue;
                }

                bool is_sep = false;
                if (c == L'\n' || c == L';') is_sep = true;
                else if (c == L'|') is_sep = true;
                else if (shell == 1 && c == L'&') is_sep = true;

                if (is_sep) {
                    while (i + 1 < script.size() && script[i + 1] == c) ++i;
                    if (!cur.empty()) {
                        size_t a = cur.find_first_not_of(L" \t\r");
                        size_t b = cur.find_last_not_of(L" \t\r");
                        if (a != std::wstring::npos) {
                            out.push_back(cur.substr(a, b - a + 1));
                        }
                    }
                    cur.clear();
                }
                else {
                    cur.push_back(c);
                }
            }

            if (!cur.empty()) {
                size_t a = cur.find_first_not_of(L" \t\r");
                size_t b = cur.find_last_not_of(L" \t\r");
                if (a != std::wstring::npos) {
                    out.push_back(cur.substr(a, b - a + 1));
                }
            }

            return out;
        }

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
                }
                else if (c == L'"' || c == L'\'') {
                    in_q = true;
                    q = c;
                    cur.push_back(c);
                }
                else if (c == L' ' || c == L'\t') {
                    if (!cur.empty()) {
                        toks.push_back(cur);
                        cur.clear();
                    }
                }
                else {
                    cur.push_back(c);
                }
            }

            if (!cur.empty()) toks.push_back(cur);
            if (toks.empty()) return p;

            std::wstring cmd_lc = toks[0];
            for (auto& ch : cmd_lc) {
                if (ch >= L'A' && ch <= L'Z') {
                    ch = ch - L'A' + L'a';
                }
            }

            p.command = cmd_lc;
            p.args.assign(toks.begin() + 1, toks.end());
            return p;
        }
    }

    void Phase5UnquoteAndReanalyze(
        int shell_id,
        ThreatResult& result,
        int depth,
        size_t scan_start)
    {
        if (depth >= 2) {
            return;
        }

        std::vector<CommandMatch> extras;
        const size_t scan_end = result.commands.size();
        for (size_t i = scan_start; i < scan_end; ++i) {
            const auto& cm = result.commands[i];
            ExecutorMatch em{};
            if (!DetectExecutor(cm, em)) {
                continue;
            }

            auto segs = SplitChain(em.inner, em.sub_shell);
            for (auto& seg : segs) {
                auto parsed = SegmentToParsed(seg);
                if (parsed.command.empty()) {
                    continue;
                }

                CommandMatch sub_cm = AnalyzeCommand(parsed, em.sub_shell);
                sub_cm.lineIndex = (size_t)-1;
                sub_cm.phase = 5;
                extras.push_back(std::move(sub_cm));
            }
        }

        if (extras.empty()) {
            return;
        }

        const size_t new_start = result.commands.size();
        for (auto& command : extras) {
            result.commands.push_back(std::move(command));
        }

        Phase5UnquoteAndReanalyze(shell_id, result, depth + 1, new_start);
    }
}
