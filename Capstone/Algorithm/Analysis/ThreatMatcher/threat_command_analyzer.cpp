#include "threat_matcher_internal.h"
#include "threat_db_connect.h"

#include <algorithm>
#include <regex>
#include <set>
#include <utility>

namespace ThreatMatcher::Internal
{
    namespace
    {
        bool IsFlag(const std::wstring& s)
        {
            if (s.empty()) return false;
            wchar_t c = s[0];
            return c == L'-' || c == L'/' || c == L'+';
        }

        // 명령 인자를 (param, value) 단위로 정리한다.
        // 예: -ExecutionPolicy Bypass -> ("-ExecutionPolicy", "Bypass")
        std::vector<std::pair<std::wstring, std::wstring>>
        SplitParamValuePairs(const std::vector<std::wstring>& args)
        {
            std::vector<std::pair<std::wstring, std::wstring>> out;
            out.reserve(args.size());

            for (size_t i = 0; i < args.size();) {
                const auto& a = args[i];
                if (IsFlag(a)) {
                    const auto colon = a.find(L':');
                    if (colon != std::wstring::npos && colon > 1) {
                        out.emplace_back(a.substr(0, colon), a.substr(colon + 1));
                        ++i;
                        continue;
                    }

                    if (i + 1 < args.size() && !IsFlag(args[i + 1])) {
                        out.emplace_back(a, args[i + 1]);
                        i += 2;
                    }
                    else {
                        out.emplace_back(a, std::wstring{});
                        ++i;
                    }
                }
                else {
                    out.emplace_back(std::wstring{}, a);
                    ++i;
                }
            }

            return out;
        }

        TokenMatch AnalyzeToken(
            int command_id,
            const std::wstring& cmd_name,
            const std::wstring& param,
            const std::wstring& value)
        {
            TokenMatch tk{};
            tk.param = param;
            tk.value = value;
            tk.axis = L'X';

            if (!param.empty()) {
                auto r = DB::LookupCommonParameter(param);
                if (r.found) {
                    tk.tag_key = r.tag_key;
                    tk.axis = r.axis;
                    tk.base_score = r.base_score;
                    tk.from_common = true;
                    tk.in_db = true;
                    tk.matched = (r.axis != L'X');
                }
            }

            if (!tk.matched && !param.empty() && command_id > 0) {
                auto r = DB::LookupParameter(command_id, param);
                if (r.found) {
                    tk.tag_key = r.tag_key;
                    tk.axis = r.axis;
                    tk.base_score = r.base_score;
                    tk.in_db = true;
                    tk.matched = (r.axis != L'X');
                }
            }

            if (!value.empty()) {
                std::wstring lookup_value = value;
                auto cut = lookup_value.find_first_of(L")}");
                if (cut != std::wstring::npos) {
                    lookup_value = lookup_value.substr(0, cut);
                }
                if (!lookup_value.empty()) {
                    tk.enum_adjust = DB::LookupEnumAdjust(cmd_name, param, lookup_value);
                }
            }

            tk.token_score = (std::max)(0.0, tk.base_score + tk.enum_adjust);
            return tk;
        }

        void ApplyCommandSelf(int command_id, CommandMatch& cm)
        {
            for (const auto& r : DB::LookupCommandSelf(command_id)) {
                TokenMatch tk{};
                tk.tag_key = r.tag_key;
                tk.axis = r.axis;
                tk.base_score = r.base_score;
                tk.token_score = r.base_score;
                tk.matched = (r.axis != L'X');
                cm.tokens.push_back(std::move(tk));
                cm.cmd_raw += r.base_score;
            }
        }
    }

    CommandMatch AnalyzeCommand(const KeyDecoder::ParsedCommand& parsed, int shell_id)
    {
        CommandMatch cm{};
        cm.command = parsed.command;
        cm.lineIndex = parsed.lineIndex;
        cm.phase = 1;

        std::wstring next_token = parsed.args.empty() ? L"" : parsed.args[0];
        bool used_next = false;
        int cid = DB::LookupCommand(parsed.command, shell_id, next_token, &used_next);
        cm.in_db = (cid > 0);
        if (cid <= 0) {
            return cm;
        }

        const std::vector<std::wstring> eff_args =
            used_next ? std::vector<std::wstring>(parsed.args.begin() + 1, parsed.args.end())
                      : parsed.args;
        if (used_next) {
            cm.command = parsed.command + L" " + parsed.args[0];
        }

        if (eff_args.empty()) {
            ApplyCommandSelf(cid, cm);
            return cm;
        }

        auto pairs = SplitParamValuePairs(eff_args);
        int recognized_pairs = 0;
        for (auto& pv : pairs) {
            TokenMatch tk = AnalyzeToken(cid, cm.command, pv.first, pv.second);
            if (tk.in_db) {
                ++recognized_pairs;
            }
            cm.cmd_raw += tk.token_score;
            cm.tokens.push_back(std::move(tk));
        }

        if (recognized_pairs == 0) {
            ApplyCommandSelf(cid, cm);
        }

        return cm;
    }

    void Phase4FullTextScan(
        const KeyDecoder::DecodeResult& decoded,
        int shell_id,
        ThreatResult& result)
    {
        if (decoded.fullText.empty()) {
            return;
        }

        std::set<int> already_ids;
        for (const auto& cm : result.commands) {
            int cid = DB::LookupCommand(cm.command, shell_id);
            if (cid > 0) {
                already_ids.insert(cid);
            }
        }

        std::wregex pat(LR"(\b([A-Z][a-zA-Z]+)-([A-Z][a-zA-Z]+)\b)");
        auto begin = std::wsregex_iterator(decoded.fullText.begin(), decoded.fullText.end(), pat);
        auto end = std::wsregex_iterator();

        for (auto it = begin; it != end; ++it) {
            std::wstring name = it->str();
            for (auto& c : name) {
                if (c >= L'A' && c <= L'Z') {
                    c = c - L'A' + L'a';
                }
            }

            int cid = DB::LookupCommand(name, shell_id);
            if (cid <= 0 || already_ids.count(cid)) {
                continue;
            }
            already_ids.insert(cid);

            CommandMatch cm{};
            cm.command = name;
            cm.lineIndex = (size_t)-1;
            cm.in_db = true;
            cm.phase = 4;
            ApplyCommandSelf(cid, cm);

            result.commands.push_back(std::move(cm));
        }
    }
}
