#include "key_normalizer.h"

namespace KeyNormalizer
{
    // LLKHF_UP: 키 릴리즈 이벤트 플래그 (WH_KEYBOARD_LL flags 필드)
    static constexpr DWORD FLAG_KEYUP = 0x80;

    static bool IsKeyUp(const KeyboardHook::KeyRecord& r)
    {
        return (r.flags & FLAG_KEYUP) != 0;
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
                    std::wstring combo;
                    if (winDown)  combo += L"Win+";
                    if (altDown)  combo += L"Alt+";
                    combo += L"Enter";
                    size_t lineIdx = result.lines.size() - 1;
                    result.hotkeys.push_back({ combo, lineIdx });
                } else {
                    // Ctrl+Enter, Shift+Enter, 단독 Enter -> 줄바꿈
                    result.lines.push_back(L"");
                }
                continue;
            }
        }

        return result;
    }
}
