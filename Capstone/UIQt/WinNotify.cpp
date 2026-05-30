#include "WinNotify.h"

#include <windows.h>
#include <shellapi.h>
#include <string>
#include <cstring>

#pragma comment(lib, "Shell32.lib")
#pragma comment(lib, "User32.lib")

namespace WinNotify
{
    namespace
    {
        constexpr const wchar_t* kClassName = L"AbangAllWinNotifyClass";
        constexpr UINT           kIconId    = 0xBA00;  // 임의의 고유 ID

        HWND g_hwnd = nullptr;
        bool g_registered = false;

        bool ensureInitialized()
        {
            if (g_hwnd != nullptr)
                return true;

            HINSTANCE hInst = GetModuleHandleW(nullptr);

            if (!g_registered) {
                WNDCLASSEXW wc{};
                wc.cbSize        = sizeof(wc);
                wc.lpfnWndProc   = DefWindowProcW;
                wc.hInstance     = hInst;
                wc.lpszClassName = kClassName;
                RegisterClassExW(&wc);  // 이미 등록돼있어도 무해
                g_registered = true;
            }

            // message-only window — 화면에 보이지 않음.
            g_hwnd = CreateWindowExW(
                0, kClassName, L"", 0,
                0, 0, 0, 0,
                HWND_MESSAGE,    // message-only
                nullptr, hInst, nullptr);
            if (!g_hwnd)
                return false;

            // 트레이 아이콘 등록 (NIS_HIDDEN — 사용자에게 보이지 않음).
            // 알림 메시지는 NIM_MODIFY 로 따로 발사한다.
            NOTIFYICONDATAW nid{};
            nid.cbSize       = sizeof(nid);
            nid.hWnd         = g_hwnd;
            nid.uID          = kIconId;
            nid.uFlags       = NIF_ICON | NIF_STATE | NIF_TIP;
            nid.dwState      = NIS_HIDDEN;
            nid.dwStateMask  = NIS_HIDDEN;
            nid.hIcon        = LoadIconW(nullptr, IDI_INFORMATION);
            wcscpy_s(nid.szTip, ARRAYSIZE(nid.szTip), L"Abang-All");
            Shell_NotifyIconW(NIM_ADD, &nid);

            // Vista+ 권장 버전 설정 (안 해도 동작, 해두면 새 알림 동작 보장).
            nid.uVersion = NOTIFYICON_VERSION_4;
            Shell_NotifyIconW(NIM_SETVERSION, &nid);

            return true;
        }

        void copyTruncated(wchar_t* dst, size_t cap, const std::wstring& src)
        {
            if (cap == 0) return;
            size_t n = src.size();
            if (n >= cap) n = cap - 1;
            std::memcpy(dst, src.data(), n * sizeof(wchar_t));
            dst[n] = L'\0';
        }
    } // anonymous namespace

    void notify(const QString& title, const QString& body, bool silent, int timeoutMs)
    {
        if (!ensureInitialized())
            return;

        NOTIFYICONDATAW nid{};
        nid.cbSize       = sizeof(nid);
        nid.hWnd         = g_hwnd;
        nid.uID          = kIconId;
        nid.uFlags       = NIF_INFO;
        nid.dwInfoFlags  = NIIF_WARNING;
        if (silent)
            nid.dwInfoFlags |= NIIF_NOSOUND;
        nid.uTimeout     = static_cast<UINT>(timeoutMs > 0 ? timeoutMs : 5000);

        const std::wstring t = title.toStdWString();
        const std::wstring b = body.toStdWString();
        copyTruncated(nid.szInfoTitle, ARRAYSIZE(nid.szInfoTitle), t);
        copyTruncated(nid.szInfo,      ARRAYSIZE(nid.szInfo),      b);

        Shell_NotifyIconW(NIM_MODIFY, &nid);
    }

    void cleanup()
    {
        if (g_hwnd) {
            NOTIFYICONDATAW nid{};
            nid.cbSize = sizeof(nid);
            nid.hWnd   = g_hwnd;
            nid.uID    = kIconId;
            Shell_NotifyIconW(NIM_DELETE, &nid);
            DestroyWindow(g_hwnd);
            g_hwnd = nullptr;
        }
    }
}
