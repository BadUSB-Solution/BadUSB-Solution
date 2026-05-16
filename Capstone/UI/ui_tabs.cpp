#include "ui_tabs.h"

#include <commctrl.h>

#include "logger.h"
#include "ui_policy_tab.h"

#pragma comment(lib, "Comctl32.lib")

namespace UiTabs
{
    static HWND g_tabWnd = nullptr;
    static HWND g_tabStatus = nullptr;
    static HWND g_tabPolicy = nullptr;
    static HWND g_tabLogs = nullptr;

    static void SwitchTab(int idx)
    {
        if (!g_tabStatus || !g_tabPolicy || !g_tabLogs) {
            return;
        }

        ShowWindow(g_tabStatus, idx == 0 ? SW_SHOW : SW_HIDE);
        ShowWindow(g_tabPolicy, idx == 1 ? SW_SHOW : SW_HIDE);
        ShowWindow(g_tabLogs, idx == 2 ? SW_SHOW : SW_HIDE);

        if (idx == 1) {
            UiPolicyTab::Refresh();
        }
        if (idx == 2) {
            UpdateLogs();
        }
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
            hWnd, nullptr, hInst, nullptr);

        TCITEMW ti{ TCIF_TEXT };
        ti.pszText = (LPWSTR)L"Status";
        TabCtrl_InsertItem(g_tabWnd, 0, &ti);
        ti.pszText = (LPWSTR)L"Policy";
        TabCtrl_InsertItem(g_tabWnd, 1, &ti);
        ti.pszText = (LPWSTR)L"Logs";
        TabCtrl_InsertItem(g_tabWnd, 2, &ti);

        RECT trc;
        GetClientRect(g_tabWnd, &trc);
        TabCtrl_AdjustRect(g_tabWnd, FALSE, &trc);

        g_tabStatus = CreateWindowW(
            L"STATIC",
            L"BadUSB Guard Demo\n\n"
            L"- Tray resident like antivirus\n"
            L"- HID insert -> approval popup\n"
            L"- One USB -> one approval (VID/PID)\n"
            L"- Policy tab can re-allow disabled blacklist devices\n"
            L"- Logs in Logs tab\n",
            WS_CHILD | WS_VISIBLE,
            trc.left + 10, trc.top + 10,
            trc.right - trc.left - 20,
            trc.bottom - trc.top - 20,
            g_tabWnd, nullptr, hInst, nullptr);

        g_tabPolicy = UiPolicyTab::Create(g_tabWnd, hInst, trc);

        g_tabLogs = CreateWindowExW(
            WS_EX_CLIENTEDGE, L"EDIT", L"",
            WS_CHILD | WS_VISIBLE | WS_VSCROLL |
            ES_MULTILINE | ES_READONLY | ES_AUTOVSCROLL,
            trc.left + 6, trc.top + 6,
            trc.right - trc.left - 12,
            trc.bottom - trc.top - 12,
            g_tabWnd, nullptr, hInst, nullptr);

        ShowWindow(g_tabPolicy, SW_HIDE);
        ShowWindow(g_tabLogs, SW_HIDE);
    }

    void OnSelChange()
    {
        if (!g_tabWnd) {
            return;
        }

        int idx = TabCtrl_GetCurSel(g_tabWnd);
        SwitchTab(idx);
    }

    bool HandleCommand(WPARAM wParam, LPARAM lParam)
    {
        return UiPolicyTab::HandleCommand(wParam, lParam);
    }

    bool IsLogsTabActive()
    {
        if (!g_tabWnd) {
            return false;
        }
        return TabCtrl_GetCurSel(g_tabWnd) == 2;
    }

    void UpdateLogs()
    {
        if (!g_tabLogs) {
            return;
        }

        std::wstring all = Logger::GetAll();
        SetWindowTextW(g_tabLogs, all.c_str());
    }
}
