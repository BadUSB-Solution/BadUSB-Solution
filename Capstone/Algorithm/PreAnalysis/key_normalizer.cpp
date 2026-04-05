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

    NormalizeResult Normalize(const std::vector<KeyboardHook::KeyRecord>& buf)
    {
        NormalizeResult result;
        result.lines.push_back(L"");  // 첫 번째 빈 줄로 시작

        // 현재 눌린 modifier 상태
        bool shiftDown = false;
        bool ctrlDown  = false;
        bool altDown   = false;
        bool winDown   = false;

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
                    // Ctrl+Enter는 줄바꿈으로 처리 (ref에 없고 BadUSB와 무관)
                    uint8_t mods = BuildMods(winDown, false, altDown, false);
                    auto def = KeyShortcutRef::Lookup(mods, vk);
                    std::wstring combo = def ? def->combo
                                             : BuildComboFallback(winDown, false, altDown, false, vk);
                    result.hotkeys.push_back({ combo, result.lines.size() - 1 });
                } else {
                    // Ctrl+Enter, Shift+Enter, 단독 Enter -> 줄바꿈
                    result.lines.push_back(L"");
                }
                continue;
            }

            // --- Win/Ctrl/Alt + 일반 키 -> HotkeyEvent ---
            // Shift 단독은 문자 변환 modifier이므로 단축키 아님
            if (winDown || ctrlDown || altDown) {
                uint8_t mods = BuildMods(winDown, ctrlDown, altDown, shiftDown);
                auto def = KeyShortcutRef::Lookup(mods, vk);
                std::wstring combo = def ? def->combo
                                         : BuildComboFallback(winDown, ctrlDown, altDown, shiftDown, vk);
                result.hotkeys.push_back({ combo, result.lines.size() - 1 });
                continue;
            }

            // 나머지 키: 이후 단계에서 문자 변환 예정 (현재는 무시)
        }

        return result;
    }
}
