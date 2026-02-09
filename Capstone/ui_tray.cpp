// 백그라운드 실행을 위한 트레이 ui 부분

#include "ui_tray.h"
#include <shellapi.h>
#include "ui_message_id.h"

#ifndef ARRAYSIZE
#define ARRAYSIZE(A) (sizeof(A)/sizeof((A)[0]))
#endif

namespace UiTray
{
    void AddIcon(HWND hWnd, HINSTANCE hInst, int iconResId)
    {
        NOTIFYICONDATAW nid{};
        nid.cbSize = sizeof(nid);
        nid.hWnd = hWnd;
        nid.uID = TRAY_ICON_ID;
        nid.uFlags = NIF_MESSAGE | NIF_ICON | NIF_TIP;
        nid.uCallbackMessage = WM_TRAYICON;
        nid.hIcon = LoadIconW(hInst, MAKEINTRESOURCE(iconResId));
        lstrcpynW(nid.szTip, L"BadUSB Guard (Demo)", ARRAYSIZE(nid.szTip));
        Shell_NotifyIconW(NIM_ADD, &nid);
    }

    void RemoveIcon(HWND hWnd)
    {
        NOTIFYICONDATAW nid{};
        nid.cbSize = sizeof(nid);
        nid.hWnd = hWnd;
        nid.uID = TRAY_ICON_ID;
        Shell_NotifyIconW(NIM_DELETE, &nid);
    }

    void ShowMenu(HWND hWnd)
    {
        POINT pt;
        GetCursorPos(&pt);

        HMENU menu = CreatePopupMenu();
        AppendMenuW(menu, MF_STRING, IDM_TRAY_OPEN, L"Open");
        AppendMenuW(menu, MF_STRING, IDM_TRAY_EXIT, L"Exit");

        SetForegroundWindow(hWnd);
        TrackPopupMenu(menu, TPM_RIGHTBUTTON, pt.x, pt.y, 0, hWnd, nullptr);
        DestroyMenu(menu);
    }
}
