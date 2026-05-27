#include "keyboard_hook.h"
#include <mutex>
#include "logger.h"

namespace KeyboardHook
{
    static HHOOK g_hook = nullptr;
    static std::vector<KeyRecord> g_buffer;
    static std::mutex g_mutex;
    static const ULONG_PTR MARKER_PASSTHROUGH = 0xBADC0DE;

    static LRESULT CALLBACK HookProc(int nCode, WPARAM wParam, LPARAM lParam)
    {
        if (nCode < 0)
            return CallNextHookEx(g_hook, nCode, wParam, lParam);

        auto* kb = reinterpret_cast<KBDLLHOOKSTRUCT*>(lParam);

        // 마커가 붙은 입력은 통과 (추후 선택적 차단 확장용)
        if (kb->dwExtraInfo == MARKER_PASSTHROUGH)
            return CallNextHookEx(g_hook, nCode, wParam, lParam);

        // 버퍼에 기록
        {
            std::lock_guard<std::mutex> lock(g_mutex);
            g_buffer.push_back({ kb->vkCode, kb->scanCode, kb->flags, kb->time });
        }

        // return 1: 시스템에 전달하지 않음 (차단)
        return 1;
    }

    bool Install()
    {
        if (g_hook != nullptr)
            return false;

        ClearBuffer();

        g_hook = SetWindowsHookExW(
            WH_KEYBOARD_LL,
            HookProc,
            GetModuleHandle(NULL),
            0
        );

        if (g_hook) {
            Logger::Msg::HookInstalled();
            return true;
        }

        Logger::Msg::HookInstallFailed();
        return false;
    }

    // Hook 해제 후 modifier 키 stuck 방지: 강제 KEYUP 전송
    static void ResetKeyboardState()
    {
        static const BYTE modifiers[] = {
            VK_LSHIFT, VK_RSHIFT,
            VK_LCONTROL, VK_RCONTROL,
            VK_LMENU, VK_RMENU,
            VK_LWIN, VK_RWIN,
        };

        for (BYTE vk : modifiers)
            keybd_event(vk, 0, KEYEVENTF_KEYUP, 0);
    }

    void Uninstall()
    {
        if (g_hook == nullptr)
            return;

        UnhookWindowsHookEx(g_hook);
        g_hook = nullptr;

        ResetKeyboardState();

        Logger::Msg::HookRemoved();
    }

    bool IsActive()
    {
        return g_hook != nullptr;
    }

    std::vector<KeyRecord> GetBufferedKeys()
    {
        std::lock_guard<std::mutex> lock(g_mutex);
        return g_buffer;
    }

    void ClearBuffer()
    {
        std::lock_guard<std::mutex> lock(g_mutex);
        g_buffer.clear();
    }
}
