#include "test_key_parser.h"
#include <windows.h>
#include <unordered_map>
#include <sstream>
#include <cwctype>

namespace TestKeyParser
{
    static constexpr DWORD KEYUP_FLAG = 0x80;  // LLKHF_UP

    struct TokenInfo {
        bool  found       = false;
        bool  is_modifier = false;
        DWORD vk          = 0;
    };

    static const std::unordered_map<std::wstring, TokenInfo>& KeywordMap()
    {
        static const std::unordered_map<std::wstring, TokenInfo> m = {
            // Modifiers
            {L"win",       {true, true,  VK_LWIN}},
            {L"ctrl",      {true, true,  VK_CONTROL}},
            {L"alt",       {true, true,  VK_MENU}},
            {L"shift",     {true, true,  VK_SHIFT}},

            // Named keys
            {L"space",     {true, false, VK_SPACE}},
            {L"enter",     {true, false, VK_RETURN}},
            {L"return",    {true, false, VK_RETURN}},
            {L"tab",       {true, false, VK_TAB}},
            {L"backspace", {true, false, VK_BACK}},
            {L"bksp",      {true, false, VK_BACK}},
            {L"esc",       {true, false, VK_ESCAPE}},
            {L"escape",    {true, false, VK_ESCAPE}},
            {L"delete",    {true, false, VK_DELETE}},
            {L"del",       {true, false, VK_DELETE}},
            {L"insert",    {true, false, VK_INSERT}},
            {L"ins",       {true, false, VK_INSERT}},
            {L"home",      {true, false, VK_HOME}},
            {L"end",       {true, false, VK_END}},
            {L"pgup",      {true, false, VK_PRIOR}},
            {L"pgdn",      {true, false, VK_NEXT}},
            {L"up",        {true, false, VK_UP}},
            {L"down",      {true, false, VK_DOWN}},
            {L"left",      {true, false, VK_LEFT}},
            {L"right",     {true, false, VK_RIGHT}},
            {L"capslock",  {true, false, VK_CAPITAL}},
        };
        return m;
    }

    static const std::unordered_map<wchar_t, DWORD>& SymbolMap()
    {
        static const std::unordered_map<wchar_t, DWORD> m = {
            {L'-',  VK_OEM_MINUS},
            {L'=',  VK_OEM_PLUS},
            {L'[',  VK_OEM_4},
            {L']',  VK_OEM_6},
            {L'\\', VK_OEM_5},
            {L';',  VK_OEM_1},
            {L'\'', VK_OEM_7},
            {L',',  VK_OEM_COMMA},
            {L'.',  VK_OEM_PERIOD},
            {L'/',  VK_OEM_2},
            {L'`',  VK_OEM_3},
        };
        return m;
    }

    static std::wstring ToLower(const std::wstring& s)
    {
        std::wstring r;
        r.reserve(s.size());
        for (auto c : s) r.push_back((wchar_t)towlower(c));
        return r;
    }

    static TokenInfo Resolve(const std::wstring& tokRaw)
    {
        if (tokRaw.empty()) return {};

        std::wstring tok = ToLower(tokRaw);

        // 1) 키워드 매핑
        auto& kmap = KeywordMap();
        auto it = kmap.find(tok);
        if (it != kmap.end()) return it->second;

        // 2) F1~F12
        if (tok.size() >= 2 && tok.size() <= 3 && tok[0] == L'f') {
            int n = 0;
            for (size_t i = 1; i < tok.size(); ++i) {
                if (tok[i] < L'0' || tok[i] > L'9') { n = -1; break; }
                n = n * 10 + (tok[i] - L'0');
            }
            if (n >= 1 && n <= 12)
                return { true, false, (DWORD)(VK_F1 + n - 1) };
        }

        // 3) 단일 글자 알파벳
        if (tok.size() == 1) {
            wchar_t c = tok[0];
            if (c >= L'a' && c <= L'z')
                return { true, false, (DWORD)(L'A' + (c - L'a')) };
            if (c >= L'0' && c <= L'9')
                return { true, false, (DWORD)c };

            // 4) 단일 기호
            auto& smap = SymbolMap();
            auto sit = smap.find(c);
            if (sit != smap.end())
                return { true, false, sit->second };
        }

        return {};  // 해석 실패
    }

    static std::vector<std::wstring> SplitWhitespace(const std::wstring& s)
    {
        std::vector<std::wstring> out;
        std::wstring cur;
        for (auto c : s) {
            if (c == L' ' || c == L'\t' || c == L'\r' || c == L'\n') {
                if (!cur.empty()) { out.push_back(cur); cur.clear(); }
            } else {
                cur.push_back(c);
            }
        }
        if (!cur.empty()) out.push_back(cur);
        return out;
    }

    std::vector<KeyboardHook::KeyRecord>
    ParseSequence(const std::wstring& tokens, const Options& opts)
    {
        std::vector<KeyboardHook::KeyRecord> out;
        auto split = SplitWhitespace(tokens);

        std::vector<DWORD> pendingMods;  // 다음 키 직전까지 보류된 modifier VK
        DWORD time = opts.startTime;

        auto emit = [&](DWORD vk, DWORD flags, DWORD t) {
            KeyboardHook::KeyRecord r{};
            r.vkCode   = vk;
            r.scanCode = 0;
            r.flags    = flags;
            r.time     = t;
            out.push_back(r);
        };

        for (const auto& tokRaw : split) {
            TokenInfo info = Resolve(tokRaw);
            if (!info.found) continue;  // 알 수 없는 토큰 무시

            if (info.is_modifier) {
                pendingMods.push_back(info.vk);
                continue;
            }

            // Non-modifier 키 - 보류된 modifier 적용 후 키 emit, 그 후 modifier 해제
            for (DWORD m : pendingMods)
                emit(m, 0, time);

            emit(info.vk, 0, time);

            // modifier 해제는 키 emit 직후 (역순)
            for (auto it = pendingMods.rbegin(); it != pendingMods.rend(); ++it)
                emit(*it, KEYUP_FLAG, time);

            pendingMods.clear();
            time += opts.intervalMs;
        }

        return out;
    }
}
