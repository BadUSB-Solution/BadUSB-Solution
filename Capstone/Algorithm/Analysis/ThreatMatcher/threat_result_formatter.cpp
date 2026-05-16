#include "threat_matcher_internal.h"

#include <algorithm>
#include <cmath>
#include <iomanip>
#include <sstream>

namespace ThreatMatcher::Internal
{
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
                }
                else {
                    ++result.unmatched_tokens;
                }
            }
            result.session_raw += cm.cmd_raw;
        }

        const size_t n = result.activated_tags.size();
        if (n > 0) {
            double sq = std::sqrt((double)n);
            result.session_score = (std::min)(10.0, result.session_raw / sq);
        }
        else {
            result.session_score = 0.0;
        }
    }

    namespace
    {
        std::wstring TokenLabel(const TokenMatch& tk)
        {
            if (tk.param.empty() && tk.value.empty()) return L"(command-self)";
            if (tk.param.empty()) return L"<positional> " + tk.value;
            if (tk.value.empty()) return tk.param;
            return tk.param + L" " + tk.value;
        }

        std::wstring FormatToken(const TokenMatch& tk)
        {
            std::wostringstream o;
            o << std::fixed << std::setprecision(2);

            const std::wstring tag = tk.tag_key.empty()
                ? L"<unmatched>"
                : tk.tag_key + L" · " + std::wstring(1, tk.axis)
                  + (tk.from_common ? L" common" : L"");

            o << L"  - " << TokenLabel(tk) << L"\r\n"
              << L"    [" << tag << L"] "
              << tk.base_score
              << (tk.enum_adjust >= 0 ? L" + " : L" - ")
              << std::abs(tk.enum_adjust)
              << L" = " << tk.token_score << L"\r\n";
            return o.str();
        }
    }

    std::wstring FormatResultLog(const ThreatResult& result)
    {
        std::wostringstream o;
        o << std::fixed << std::setprecision(2);
        const std::wstring DBL_SEP(40, L'=');
        const std::wstring SEC_SEP(28, L'-');

        o << DBL_SEP << L"\r\n"
          << L" ThreatMatcher Analysis\n"
          << DBL_SEP << L"\r\n";

        if (result.commands.empty()) {
            o << L"\n (no commands to analyze)\n";
        }

        size_t cmd_idx = 0;
        for (const auto& cm : result.commands) {
            ++cmd_idx;
            o << L"\r\n[" << cmd_idx << L"] ";
            if (cm.phase == 5) {
                o << L"line=quoted";
            }
            else if (cm.lineIndex == (size_t)-1) {
                o << L"line=fullText";
            }
            else {
                o << L"line=" << cm.lineIndex;
            }
            o << L"  " << cm.command
              << (cm.in_db ? L"  [DB]" : L"  [no DB match]") << L"\r\n";

            if (cm.tokens.empty()) {
                o << L"  - no tokens\r\n";
                continue;
            }

            for (const auto& tk : cm.tokens) {
                o << FormatToken(tk);
            }

            o << L"  " << SEC_SEP << L"\r\n"
              << L"  cmd_raw = "
              << std::fixed << std::setprecision(2) << cm.cmd_raw << L"\r\n";
        }

        o << L"\r\n" << DBL_SEP << L"\r\n"
          << L" Session Summary\n"
          << DBL_SEP << L"\r\n"
          << L"\n Token counts\n"
          << L"  total    : " << result.total_tokens << L"\r\n"
          << L"  matched  : " << result.matched_tokens << L"\r\n"
          << L"  unmatched: " << result.unmatched_tokens << L"\r\n"
          << L"\n Activated tags (" << result.activated_tags.size() << L")\n";

        if (result.activated_tags.empty()) {
            o << L"  (none)\n";
        }
        else {
            for (const auto& t : result.activated_tags) {
                o << L"  - " << t << L"\r\n";
            }
        }

        o << L"\n SCIA distribution\n"
          << L"  S=" << result.axis_count_s
          << L"  C=" << result.axis_count_c
          << L"  I=" << result.axis_count_i
          << L"  A=" << result.axis_count_a << L"\r\n"
          << L"\n Score\n"
          << L"  raw = "
          << std::fixed << std::setprecision(2) << result.session_raw << L"\r\n";

        const size_t n = result.activated_tags.size();
        if (n > 0) {
            const double sq = std::sqrt((double)n);
            o << L"  N   = " << n << L"\r\n"
              << L"  sqrt(N) = "
              << std::fixed << std::setprecision(3) << sq << L"\r\n"
              << L"  " << SEC_SEP << L"\r\n"
              << L"  session_score = "
              << std::fixed << std::setprecision(3) << result.session_score
              << L"\r\n";
        }
        else {
            o << L"  session_score = "
              << std::fixed << std::setprecision(3) << result.session_score
              << L"  (no activated tags)\n";
        }

        o << DBL_SEP << L"\r\n";
        return o.str();
    }
}
