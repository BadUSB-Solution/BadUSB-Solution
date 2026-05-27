#include "ui_tabs.h"

#include <commctrl.h>

#include "logger.h"
#include "ui_policy_tab.h"
#include "ui_settings_tab.h"
#include "obfuscate.h"

#pragma comment(lib, "Comctl32.lib")

namespace UiTabs
{
    static HWND g_tabWnd = nullptr;
    static HWND g_tabStatus = nullptr;
    static HWND g_tabPolicy = nullptr;
    static HWND g_tabSettings = nullptr;
    static HWND g_tabLogs = nullptr;

    static void SwitchTab(int idx)
    {
        if (!g_tabStatus || !g_tabPolicy || !g_tabSettings || !g_tabLogs) {
            return;
        }

        ShowWindow(g_tabStatus,   idx == 0 ? SW_SHOW : SW_HIDE);
        ShowWindow(g_tabPolicy,   idx == 1 ? SW_SHOW : SW_HIDE);
        ShowWindow(g_tabSettings, idx == 2 ? SW_SHOW : SW_HIDE);
        ShowWindow(g_tabLogs,     idx == 3 ? SW_SHOW : SW_HIDE);

        if (idx == 1) {
            UiPolicyTab::Refresh();
        }
        if (idx == 2) {
            UiSettingsTab::Refresh();
        }
        if (idx == 3) {
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
        ti.pszText = (LPWSTR)L"Settings";
        TabCtrl_InsertItem(g_tabWnd, 2, &ti);
        ti.pszText = (LPWSTR)L"Logs";
        TabCtrl_InsertItem(g_tabWnd, 3, &ti);

        RECT trc;
        GetClientRect(g_tabWnd, &trc);
        TabCtrl_AdjustRect(g_tabWnd, FALSE, &trc);

        g_tabStatus = CreateWindowW(
            L"STATIC",
            AY_OBFUSCATE(L"AbangAll\n\n- Tray resident like antivirus\n- HID insert -> approval popup\n- One USB -> one approval (VID/PID)\n- Policy tab can re-allow disabled blacklist devices\n- Logs in Logs tab\n"),
            WS_CHILD | WS_VISIBLE,
            trc.left + 10, trc.top + 10,
            trc.right - trc.left - 20,
            trc.bottom - trc.top - 20,
            g_tabWnd, nullptr, hInst, nullptr);

        g_tabPolicy = UiPolicyTab::Create(g_tabWnd, hInst, trc);
        g_tabSettings = UiSettingsTab::Create(g_tabWnd, hInst, trc);

        g_tabLogs = CreateWindowExW(
            WS_EX_CLIENTEDGE, L"EDIT", L"",
            WS_CHILD | WS_VISIBLE | WS_VSCROLL |
            ES_MULTILINE | ES_READONLY | ES_AUTOVSCROLL,
            trc.left + 6, trc.top + 6,
            trc.right - trc.left - 12,
            trc.bottom - trc.top - 12,
            g_tabWnd, nullptr, hInst, nullptr);

        ShowWindow(g_tabPolicy,   SW_HIDE);
        ShowWindow(g_tabSettings, SW_HIDE);
        ShowWindow(g_tabLogs,     SW_HIDE);
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
        if (UiPolicyTab::HandleCommand(wParam, lParam)) return true;
        if (UiSettingsTab::HandleCommand(wParam, lParam)) return true;
        return false;
    }

    bool IsLogsTabActive()
    {
        if (!g_tabWnd) {
            return false;
        }
        return TabCtrl_GetCurSel(g_tabWnd) == 3;
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
