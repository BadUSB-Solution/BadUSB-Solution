// 실제 앱 화면 구성 - 현재는 매인(상태) 탭과 로그탭으로 구분

#include "ui_tabs.h"
#include <commctrl.h>
#include "logger.h"

#pragma comment(lib, "Comctl32.lib")

namespace UiTabs
{
    static HWND g_tabWnd = nullptr;
    static HWND g_tabStatus = nullptr; // 메인(상태) 탭
    static HWND g_tabLogs = nullptr; // 로그 탭

    static void SwitchTab(int idx)
    {
        if (!g_tabStatus || !g_tabLogs) return;
        ShowWindow(g_tabStatus, idx == 0 ? SW_SHOW : SW_HIDE);
        ShowWindow(g_tabLogs, idx == 1 ? SW_SHOW : SW_HIDE);
        if (idx == 1) UpdateLogs();
    }

    void Create(HWND hWnd, HINSTANCE hInst)
    {
        INITCOMMONCONTROLSEX icc{ sizeof(icc), ICC_TAB_CLASSES };
        InitCommonControlsEx(&icc);

        RECT rc;
        GetClientRect(hWnd, &rc);

        g_tabWnd = CreateWindowW(
            WC_TABCONTROLW, L"",
            WS_CHILD | WS_VISIBLE,
            10, 10, rc.right - 20, rc.bottom - 20,
            hWnd, nullptr, hInst, nullptr
        );

        TCITEMW ti{ TCIF_TEXT };
        ti.pszText = (LPWSTR)L"Status";
        TabCtrl_InsertItem(g_tabWnd, 0, &ti);
        ti.pszText = (LPWSTR)L"Logs";
        TabCtrl_InsertItem(g_tabWnd, 1, &ti);

        RECT trc;
        GetClientRect(g_tabWnd, &trc);
        TabCtrl_AdjustRect(g_tabWnd, FALSE, &trc);

        g_tabStatus = CreateWindowW(
            L"STATIC",
            L"BadUSB Guard Demo\n\n"
            L"- Tray resident like antivirus\n"
            L"- HID insert -> approval popup\n"
            L"- One USB -> one approval (VID/PID)\n"
            L"- Logs in Logs tab\n",
            WS_CHILD | WS_VISIBLE,
            trc.left + 10, trc.top + 10,
            trc.right - trc.left - 20,
            trc.bottom - trc.top - 20,
            g_tabWnd, nullptr, hInst, nullptr
        );

        g_tabLogs = CreateWindowExW(
            WS_EX_CLIENTEDGE, L"EDIT", L"",
            WS_CHILD | WS_VISIBLE | WS_VSCROLL |
            ES_MULTILINE | ES_READONLY | ES_AUTOVSCROLL,
            trc.left + 6, trc.top + 6,
            trc.right - trc.left - 12,
            trc.bottom - trc.top - 12,
            g_tabWnd, nullptr, hInst, nullptr
        );

        ShowWindow(g_tabLogs, SW_HIDE);
    }

    void OnSelChange()
    {
        if (!g_tabWnd) return;
        int idx = TabCtrl_GetCurSel(g_tabWnd);
        SwitchTab(idx);
    }

    bool IsLogsTabActive()
    {
        if (!g_tabWnd) return false;
        return TabCtrl_GetCurSel(g_tabWnd) == 1;
    }

    void UpdateLogs()
    {
        if (!g_tabLogs) return;
        std::wstring all = Logger::GetAll();
        SetWindowTextW(g_tabLogs, all.c_str());
    }
}
