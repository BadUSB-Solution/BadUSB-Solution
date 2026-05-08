#include "key_normalizer.h"
#include "keyboard_shortcuts_ref.h"

namespace KeyNormalizer
{
    // LLKHF_UP: 키 릴리즈 이벤트 플래그 (WH_KEYBOARD_LL flags 필드)
    static constexpr DWORD FLAG_KEYUP = 0x80;

    static bool IsKeyUp(const KeyboardHook::KeyRecord& r)
    {
        return (r.flags & FLAG_KEYUP) != 0;
    }

    // 현재 modifier 상태 -> KeyShortcutRef 플래그 조합
    static uint8_t BuildMods(bool win, bool ctrl, bool alt, bool shift)
    {
        uint8_t m = 0;
        if (win)   m |= KeyShortcutRef::M_WIN;
        if (ctrl)  m |= KeyShortcutRef::M_CTRL;
        if (alt)   m |= KeyShortcutRef::M_ALT;
        if (shift) m |= KeyShortcutRef::M_SHIFT;
        return m;
    }

    // Lookup 실패 시 폴백 콤보 문자열 직접 조합
    // 알파벳/숫자는 그대로, 나머지는 0xXX 형식
    static std::wstring BuildComboFallback(bool win, bool ctrl, bool alt, bool shift, DWORD vk)
    {
        std::wstring combo;
        if (win)   combo += L"Win+";
        if (ctrl)  combo += L"Ctrl+";
        if (alt)   combo += L"Alt+";
        if (shift) combo += L"Shift+";

        if ((vk >= 'A' && vk <= 'Z') || (vk >= '0' && vk <= '9')) {
            combo += (wchar_t)vk;
        } else {
            wchar_t buf[8];
            wsprintfW(buf, L"0x%02X", vk);
            combo += buf;
        }
        return combo;
    }

    // VkCode + Shift 상태 -> 문자 변환 (US 키보드 레이아웃 기준)
    // 변환 불가 키는 L'\0' 반환
    static wchar_t VkToChar(DWORD vk, bool shift)
    {
        // 알파벳 A-Z
        if (vk >= 'A' && vk <= 'Z')
            return shift ? (wchar_t)vk : (wchar_t)(vk + 32);

        // 숫자 0-9
        if (vk >= '0' && vk <= '9') {
            if (!shift) return (wchar_t)vk;
            // Shift + 숫자 → 기호 (US 키보드)
            static const wchar_t shiftNum[] = L")!@#$%^&*(";
            return shiftNum[vk - '0'];
        }

        // 공백
        if (vk == VK_SPACE) return L' ';

        // OEM 기호 키 (US 키보드 레이아웃)
        switch (vk) {
        case VK_OEM_MINUS:  return shift ? L'_'  : L'-';
        case VK_OEM_PLUS:   return shift ? L'+'  : L'=';
        case VK_OEM_1:      return shift ? L':'  : L';';
        case VK_OEM_7:      return shift ? L'"'  : L'\'';
        case VK_OEM_COMMA:  return shift ? L'<'  : L',';
        case VK_OEM_PERIOD: return shift ? L'>'  : L'.';
        case VK_OEM_2:      return shift ? L'?'  : L'/';
        case VK_OEM_4:      return shift ? L'{'  : L'[';
        case VK_OEM_6:      return shift ? L'}'  : L']';
        case VK_OEM_5:      return shift ? L'|'  : L'\\';
        case VK_OEM_3:      return shift ? L'~'  : L'`';
        }

        return L'\0';
    }

    NormalizeResult Normalize(const std::vector<KeyboardHook::KeyRecord>& buf)
    {
        NormalizeResult result;
        result.lines.push_back(L"");  // 첫 번째 빈 줄로 시작

        // 현재 눌린 modifier 상태
        bool shiftDown   = false;
        bool ctrlDown    = false;
        bool altDown     = false;
        bool winDown     = false;
        // Win+X 서브메뉴 대기 상태
        bool winXPending = false;

        for (const auto& r : buf) {
            bool up = IsKeyUp(r);
            DWORD vk = r.vkCode;

            // --- modifier 상태 업데이트 (keyup/keydown 모두 처리) ---
            if (vk == VK_LSHIFT || vk == VK_RSHIFT || vk == VK_SHIFT) {
                shiftDown = !up;
                continue;
            }
            if (vk == VK_LCONTROL || vk == VK_RCONTROL || vk == VK_CONTROL) {
                ctrlDown = !up;
                continue;
            }
            if (vk == VK_LMENU || vk == VK_RMENU || vk == VK_MENU) {
                altDown = !up;
                continue;
            }
            if (vk == VK_LWIN || vk == VK_RWIN) {
                winDown = !up;
                continue;
            }

            // keyup은 modifier 이외 키는 무시
            if (up) continue;

            // --- Enter 처리 ---
            if (vk == VK_RETURN) {
                if (winDown || altDown) {
                    // Win+Enter(내레이터), Alt+Enter(속성) -> HotkeyEvent
                    uint8_t mods = BuildMods(winDown, false, altDown, false);
                    auto def = KeyShortcutRef::Lookup(mods, vk);
                    std::wstring combo = def ? def->combo
                                             : BuildComboFallback(winDown, false, altDown, false, vk);
                    result.hotkeys.push_back({ combo, 0, result.lines.size() - 1 });
                } else {
                    // Ctrl+Enter, Shift+Enter, 단독 Enter -> 줄바꿈
                    result.lines.push_back(L"");
                }
                winXPending = false;
                continue;
            }

            // --- Win+X 서브메뉴 대기 처리 ---
            // Win+X 이후 첫 keydown을 서브메뉴 선택키로 해석
            if (winXPending) {
                winXPending = false;
                wchar_t key = (wchar_t)vk;
                wchar_t normKey = (key >= L'a' && key <= L'z') ? (key - L'a' + L'A') : key;
                auto entry = KeyShortcutRef::LookupWinXMenu(normKey);
                if (entry && !result.hotkeys.empty()) {
                    // 마지막 HotkeyEvent(Win+X)에 서브메뉴 항목 이름 추가
                    // combo\ub294 "Win+X" \uc601\ubb38 \uc2dd\ubcc4\uc790 \uadf8\ub300\ub85c \ub450\uace0, \uba54\ub274 \ud0a4\ub9cc \ubcc4\ub3c4 \ud544\ub4dc\uc5d0 \uc800\uc7a5
                    // \ud45c\uc2dc\uc6a9 action \ubb38\uc790\uc5f4\uc740 UI \uce21\uc5d0\uc11c LookupWinXMenu\ub85c \ub2e4\uc2dc \uc870\ud68c
                    result.hotkeys.back().submenuKey = normKey;
                }
                continue; // 서브메뉴 선택키는 문자로 출력하지 않음
            }

            // --- Win/Ctrl/Alt + 일반 키 -> HotkeyEvent ---
            // Shift 단독은 문자 변환 modifier이므로 단축키 아님
            if (winDown || ctrlDown || altDown) {
                uint8_t mods = BuildMods(winDown, ctrlDown, altDown, shiftDown);
                auto def = KeyShortcutRef::Lookup(mods, vk);
                std::wstring combo = def ? def->combo
                                         : BuildComboFallback(winDown, ctrlDown, altDown, shiftDown, vk);
                result.hotkeys.push_back({ combo, 0, result.lines.size() - 1 });

                // Win+X 감지 → 다음 키를 서브메뉴 선택으로 처리
                if (KeyShortcutRef::IsWinXEntry(mods, vk))
                    winXPending = true;

                continue;
            }

            // --- Backspace: 현재 줄 마지막 문자 삭제 ---
            if (vk == VK_BACK) {
                if (!result.lines.back().empty())
                    result.lines.back().pop_back();
                continue;
            }

            // --- Tab ---
            if (vk == VK_TAB) {
                result.lines.back() += L'\t';
                continue;
            }

            // --- 일반 문자 변환 ---
            wchar_t ch = VkToChar(vk, shiftDown);
            if (ch != L'\0')
                result.lines.back() += ch;
        }

        return result;
    }
}
