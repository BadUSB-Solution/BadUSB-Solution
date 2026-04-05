// usb 허용 ui 부분
#include "ui_approval.h"
#include "resource.h"

#include <commctrl.h>
#include <sstream>
#include <vector>
#include <string>
#include <cstdint>

#pragma comment(lib, "Comctl32.lib")

// -------------------------------------------
// Helpers
// -------------------------------------------
static std::wstring HexU16(uint16_t v)
{
    std::wstringstream ss;
    ss << L"0x" << std::hex << v << std::dec;
    return ss.str();
}

static std::wstring U64ToW(uint64_t v)
{
    std::wstringstream ss;
    ss << v;
    return ss.str();
}

// ListView에 넘기는 문자열 포인터의 수명 문제 해결용 캐시
// (Dialog가 살아있는 동안 유지되면 안전)
static std::vector<std::wstring> g_textCache;

static LPWSTR WSTR(const std::wstring& s)
{
    return const_cast<LPWSTR>(s.c_str());
}

static const std::wstring& Keep(std::wstring s)
{
    g_textCache.push_back(std::move(s));
    return g_textCache.back();
}

// -------------------------------------------
// ListView setup
// -------------------------------------------
static void InitListViewColumns(HWND hList)
{
    // 혹시 재호출될 수 있으니 컬럼 초기화(중복 방지)
    while (ListView_DeleteColumn(hList, 0)) {}

    LVCOLUMNW col{};
    col.mask = LVCF_TEXT | LVCF_WIDTH | LVCF_SUBITEM;

    col.pszText = (LPWSTR)L"Allow";
    col.cx = 55;
    col.iSubItem = 0;
    ListView_InsertColumn(hList, 0, &col);

    col.pszText = (LPWSTR)L"VID";
    col.cx = 75;
    col.iSubItem = 1;
    ListView_InsertColumn(hList, 1, &col);

    col.pszText = (LPWSTR)L"PID";
    col.cx = 75;
    col.iSubItem = 2;
    ListView_InsertColumn(hList, 2, &col);

    col.pszText = (LPWSTR)L"Fingerprint";
    col.cx = 180;
    col.iSubItem = 3;
    ListView_InsertColumn(hList, 3, &col);

    col.pszText = (LPWSTR)L"Name";
    col.cx = 160;
    col.iSubItem = 4;
    ListView_InsertColumn(hList, 4, &col);
}

static void FillListView(HWND hList, const std::vector<UsbItem>& devices)
{
    ListView_DeleteAllItems(hList);

    g_textCache.clear();
    g_textCache.reserve(devices.size() * 4);

    for (int i = 0; i < (int)devices.size(); i++) {
        const auto& d = devices[i];

        LVITEMW item{};
        item.mask = LVIF_TEXT;
        item.iItem = i;
        item.iSubItem = 0;
        item.pszText = (LPWSTR)L"";
        int idx = ListView_InsertItem(hList, &item);

        ListView_SetItemText(hList, idx, 1, WSTR(Keep(HexU16(d.vid))));
        ListView_SetItemText(hList, idx, 2, WSTR(Keep(HexU16(d.pid))));
        ListView_SetItemText(hList, idx, 3, WSTR(Keep(U64ToW(d.fingerprint))));
        ListView_SetItemText(hList, idx, 4, WSTR(Keep(d.name)));

        ListView_SetCheckState(hList, idx, d.allow ? TRUE : FALSE);
    }
}

static void ReadChecksBack(HWND hList, std::vector<UsbItem>& devices)
{
    for (int i = 0; i < (int)devices.size(); i++) {
        devices[i].allow = (ListView_GetCheckState(hList, i) == TRUE);
    }
}

// -------------------------------------------
// Dialog Proc
// -------------------------------------------
struct DialogState {
    std::vector<UsbItem>* devices = nullptr;
    bool saved = false;
};

static INT_PTR CALLBACK UsbApprovalDlgProc(HWND hDlg, UINT msg, WPARAM wParam, LPARAM lParam)
{
    DialogState* st = (DialogState*)GetWindowLongPtrW(hDlg, GWLP_USERDATA);

    switch (msg) {
    case WM_INITDIALOG: {
        st = (DialogState*)lParam;
        SetWindowLongPtrW(hDlg, GWLP_USERDATA, (LONG_PTR)st);

        INITCOMMONCONTROLSEX icc{ sizeof(icc), ICC_LISTVIEW_CLASSES };
        InitCommonControlsEx(&icc);

        HWND hList = GetDlgItem(hDlg, IDC_USB_LIST);

        ListView_SetExtendedListViewStyle(
            hList,
            LVS_EX_FULLROWSELECT | LVS_EX_GRIDLINES | LVS_EX_CHECKBOXES
        );

        InitListViewColumns(hList);

        if (st && st->devices) {
            FillListView(hList, *st->devices);
        }

        // 트레이 상주 앱이라 포커스 이슈 줄이려면(선택):
        // SetForegroundWindow(hDlg);
        return TRUE;
    }

    case WM_COMMAND:
        switch (LOWORD(wParam)) {
        case IDC_BTN_SAVE: {
            HWND hList = GetDlgItem(hDlg, IDC_USB_LIST);
            if (st && st->devices) {
                ReadChecksBack(hList, *st->devices);
            }
            if (st) st->saved = true;
            EndDialog(hDlg, IDOK);
            return TRUE;
        }
        case IDC_BTN_CANCEL:
            EndDialog(hDlg, IDCANCEL);
            return TRUE;
        }
        break;

    case WM_CLOSE:
        EndDialog(hDlg, IDCANCEL);
        return TRUE;
    }

    return FALSE;
}

// -------------------------------------------
// Public API
// -------------------------------------------
namespace ApprovalUI
{
    bool ShowUsbListDialog(HWND parent, std::vector<UsbItem>& devices)
    {
        DialogState st;
        st.devices = &devices;

        INT_PTR r = DialogBoxParamW(
            GetModuleHandleW(nullptr),
            MAKEINTRESOURCEW(IDD_USB_APPROVAL),
            parent,
            UsbApprovalDlgProc,
            (LPARAM)&st
        );
        (void)r;

        return st.saved;
    }
}