// usb 허용 ui 부분

//#include "ui_approval.h"
//#include <sstream>
//#include <string>
//
//static std::wstring HexU16(uint16_t v)
//{
//    std::wstringstream ss;
//    ss << L"0x" << std::hex << v << std::dec;
//    return ss.str();
//}
//
//namespace ApprovalUI
//{
//    bool AskUserApproval(HWND hWnd, uint16_t vid, uint16_t pid, uint64_t fingerprint)
//    {
//        std::wstringstream ss;
//        ss << L"Unknown HID device detected.\n\n"
//            << L"VID: " << HexU16(vid) << L"\n"
//            << L"PID: " << HexU16(pid) << L"\n"
//            << L"Fingerprint: " << fingerprint << L"\n\n"
//            << L"Allow this device?";
//
//        int r = MessageBoxW(
//            hWnd,
//            ss.str().c_str(),
//            L"BadUSB Guard",
//            MB_ICONWARNING | MB_YESNO | MB_DEFBUTTON2 | MB_TOPMOST | MB_SETFOREGROUND
//        );
//        return (r == IDYES);
//    }
//}

#include "ui_approval.h"
#include "../Core/resource.h"

#include <commctrl.h>
#include <sstream>

#pragma comment(lib, "Comctl32.lib")

static std::wstring HexU16(uint16_t v) {
    std::wstringstream ss;
    ss << L"0x" << std::hex << v << std::dec;
    return ss.str();
}

static std::wstring U64ToW(uint64_t v) {
    std::wstringstream ss;
    ss << v;
    return ss.str();
}

static void InitListViewColumns(HWND hList) {
    LVCOLUMNW col{};
    col.mask = LVCF_TEXT | LVCF_WIDTH | LVCF_SUBITEM;

    col.pszText = (LPWSTR)L"Allow";
    col.cx = 55;
    ListView_InsertColumn(hList, 0, &col);

    col.pszText = (LPWSTR)L"VID";
    col.cx = 75; col.iSubItem = 1;
    ListView_InsertColumn(hList, 1, &col);

    col.pszText = (LPWSTR)L"PID";
    col.cx = 75; col.iSubItem = 2;
    ListView_InsertColumn(hList, 2, &col);

    col.pszText = (LPWSTR)L"Fingerprint";
    col.cx = 125; col.iSubItem = 3;
    ListView_InsertColumn(hList, 3, &col);

    col.pszText = (LPWSTR)L"Name";
    col.cx = 110; col.iSubItem = 4;
    ListView_InsertColumn(hList, 4, &col);
}

static void FillListView(HWND hList, const std::vector<UsbItem>& devices) {
    ListView_DeleteAllItems(hList);

    for (int i = 0; i < (int)devices.size(); i++) {
        const auto& d = devices[i];

        LVITEMW item{};
        item.mask = LVIF_TEXT;
        item.iItem = i;
        item.iSubItem = 0;
        item.pszText = (LPWSTR)L"";
        int idx = ListView_InsertItem(hList, &item);

        ListView_SetItemText(hList, idx, 1, (LPWSTR)HexU16(d.vid).c_str());
        ListView_SetItemText(hList, idx, 2, (LPWSTR)HexU16(d.pid).c_str());
        ListView_SetItemText(hList, idx, 3, (LPWSTR)U64ToW(d.fingerprint).c_str());
        ListView_SetItemText(hList, idx, 4, (LPWSTR)d.name.c_str());

        ListView_SetCheckState(hList, idx, d.allow ? TRUE : FALSE);
    }
}

static void ReadChecksBack(HWND hList, std::vector<UsbItem>& devices) {
    for (int i = 0; i < (int)devices.size(); i++) {
        devices[i].allow = (ListView_GetCheckState(hList, i) == TRUE);
    }
}

struct DialogState {
    std::vector<UsbItem>* devices = nullptr;
    bool saved = false;
};

static INT_PTR CALLBACK UsbApprovalDlgProc(HWND hDlg, UINT msg, WPARAM wParam, LPARAM lParam) {
    DialogState* st = (DialogState*)GetWindowLongPtrW(hDlg, GWLP_USERDATA);

    switch (msg) {
    case WM_INITDIALOG: {
        st = (DialogState*)lParam;
        SetWindowLongPtrW(hDlg, GWLP_USERDATA, (LONG_PTR)st);

        INITCOMMONCONTROLSEX icc{ sizeof(icc), ICC_LISTVIEW_CLASSES };
        InitCommonControlsEx(&icc);

        HWND hList = GetDlgItem(hDlg, IDC_USB_LIST);
        ListView_SetExtendedListViewStyle(hList,
            LVS_EX_FULLROWSELECT | LVS_EX_GRIDLINES | LVS_EX_CHECKBOXES);

        InitListViewColumns(hList);
        FillListView(hList, *st->devices);
        return TRUE;
    }

    case WM_COMMAND:
        switch (LOWORD(wParam)) {
        case IDC_BTN_SAVE: {
            HWND hList = GetDlgItem(hDlg, IDC_USB_LIST);
            ReadChecksBack(hList, *st->devices);
            st->saved = true;
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

namespace ApprovalUI {
    bool ShowUsbListDialog(HWND parent, std::vector<UsbItem>& devices) {
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