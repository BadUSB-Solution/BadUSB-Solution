#include "ui_policy_tab.h"

#include <commctrl.h>

#include <cstdint>
#include <sstream>
#include <string>
#include <vector>

#include "logger.h"
#include "policy.h"
#include "../Policy/device_blocker.h"

#pragma comment(lib, "Comctl32.lib")

namespace
{
    constexpr int IDC_POLICY_LIST = 41001;
    constexpr int IDC_REFRESH = 41002;
    constexpr int IDC_ALLOW_SELECTED = 41003;

    HWND g_root = nullptr;
    HWND g_list = nullptr;
    WNDPROC g_prevRootProc = nullptr;
    std::vector<UsbItem> g_items;
    std::vector<std::wstring> g_textCache;

    HMENU ControlId(int id)
    {
        return reinterpret_cast<HMENU>(static_cast<INT_PTR>(id));
    }

    std::wstring HexU16(uint16_t value)
    {
        std::wostringstream ss;
        ss << L"0x" << std::uppercase << std::hex << value;
        return ss.str();
    }

    std::wstring U64ToW(uint64_t value)
    {
        std::wostringstream ss;
        ss << value;
        return ss.str();
    }

    LPWSTR WSTR(const std::wstring& value)
    {
        return const_cast<LPWSTR>(value.c_str());
    }

    const std::wstring& Keep(std::wstring value)
    {
        g_textCache.push_back(std::move(value));
        return g_textCache.back();
    }

    void InitColumns()
    {
        while (ListView_DeleteColumn(g_list, 0)) {}

        LVCOLUMNW col{};
        col.mask = LVCF_TEXT | LVCF_WIDTH | LVCF_SUBITEM;

        col.pszText = (LPWSTR)L"Policy";
        col.cx = 90;
        col.iSubItem = 0;
        ListView_InsertColumn(g_list, 0, &col);

        col.pszText = (LPWSTR)L"VID";
        col.cx = 80;
        col.iSubItem = 1;
        ListView_InsertColumn(g_list, 1, &col);

        col.pszText = (LPWSTR)L"PID";
        col.cx = 80;
        col.iSubItem = 2;
        ListView_InsertColumn(g_list, 2, &col);

        col.pszText = (LPWSTR)L"Fingerprint";
        col.cx = 190;
        col.iSubItem = 3;
        ListView_InsertColumn(g_list, 3, &col);

        col.pszText = (LPWSTR)L"Name";
        col.cx = 260;
        col.iSubItem = 4;
        ListView_InsertColumn(g_list, 4, &col);
    }

    void AppendItems(const std::vector<UsbItem>& source, bool allow)
    {
        for (auto item : source) {
            item.allow = allow;
            g_items.push_back(std::move(item));
        }
    }

    int SelectedIndex()
    {
        return ListView_GetNextItem(g_list, -1, LVNI_SELECTED);
    }

    LRESULT CALLBACK RootProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam)
    {
        if (msg == WM_COMMAND && UiPolicyTab::HandleCommand(wParam, lParam)) {
            return 0;
        }

        return CallWindowProcW(g_prevRootProc, hWnd, msg, wParam, lParam);
    }
}

namespace UiPolicyTab
{
    HWND Create(HWND parent, HINSTANCE hInst, const RECT& rect)
    {
        INITCOMMONCONTROLSEX icc{ sizeof(icc), ICC_LISTVIEW_CLASSES };
        InitCommonControlsEx(&icc);

        g_root = CreateWindowW(
            L"STATIC",
            L"",
            WS_CHILD | WS_VISIBLE,
            rect.left, rect.top,
            rect.right - rect.left,
            rect.bottom - rect.top,
            parent,
            nullptr,
            hInst,
            nullptr);

        g_prevRootProc = reinterpret_cast<WNDPROC>(
            SetWindowLongPtrW(g_root, GWLP_WNDPROC, reinterpret_cast<LONG_PTR>(RootProc)));

        CreateWindowW(
            L"BUTTON",
            L"Refresh",
            WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
            8, 8, 90, 28,
            g_root,
            ControlId(IDC_REFRESH),
            hInst,
            nullptr);

        CreateWindowW(
            L"BUTTON",
            L"Allow selected",
            WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
            106, 8, 130, 28,
            g_root,
            ControlId(IDC_ALLOW_SELECTED),
            hInst,
            nullptr);

        g_list = CreateWindowExW(
            WS_EX_CLIENTEDGE,
            WC_LISTVIEWW,
            L"",
            WS_CHILD | WS_VISIBLE | LVS_REPORT | LVS_SINGLESEL | WS_VSCROLL,
            8, 44,
            rect.right - rect.left - 16,
            rect.bottom - rect.top - 52,
            g_root,
            ControlId(IDC_POLICY_LIST),
            hInst,
            nullptr);

        ListView_SetExtendedListViewStyle(g_list, LVS_EX_FULLROWSELECT | LVS_EX_GRIDLINES);
        InitColumns();
        Refresh();
        return g_root;
    }

    bool HandleCommand(WPARAM wParam, LPARAM)
    {
        switch (LOWORD(wParam)) {
        case IDC_REFRESH:
            Refresh();
            return true;

        case IDC_ALLOW_SELECTED: {
            const int index = SelectedIndex();
            if (index < 0 || index >= static_cast<int>(g_items.size())) {
                Logger::Msg::PolicyNoSelection();
                return true;
            }

            const UsbItem item = g_items[index];
            Policy::AddWhitelist(item);

            std::wstring enableDetail;
            const DeviceBlocker::EnableResult enableResult =
                DeviceBlocker::EnableByVidPid(item.vid, item.pid, &enableDetail);

            Logger::Msg::PolicyAllowedSelected(item.name);
            Logger::Msg::PolicyEnableResult(static_cast<int>(enableResult), enableDetail);

            Refresh();
            return true;
        }

        default:
            return false;
        }
    }

    void Refresh()
    {
        if (!g_list) {
            return;
        }

        g_items.clear();
        AppendItems(Policy::GetWhitelist(), true);
        AppendItems(Policy::GetBlacklist(), false);

        ListView_DeleteAllItems(g_list);
        g_textCache.clear();
        g_textCache.reserve(g_items.size() * 5);

        for (int i = 0; i < static_cast<int>(g_items.size()); ++i) {
            const auto& item = g_items[i];
            LVITEMW row{};
            row.mask = LVIF_TEXT;
            row.iItem = i;
            row.iSubItem = 0;
            row.pszText = WSTR(Keep(item.allow ? L"Whitelist" : L"Blacklist"));
            const int idx = ListView_InsertItem(g_list, &row);

            ListView_SetItemText(g_list, idx, 1, WSTR(Keep(HexU16(item.vid))));
            ListView_SetItemText(g_list, idx, 2, WSTR(Keep(HexU16(item.pid))));
            ListView_SetItemText(g_list, idx, 3, WSTR(Keep(U64ToW(item.fingerprint))));
            ListView_SetItemText(g_list, idx, 4, WSTR(Keep(item.name)));
        }
    }
}
