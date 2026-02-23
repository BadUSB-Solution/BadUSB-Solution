//#pragma once
//#include <windows.h>
//#include <cstdint>
//
//namespace ApprovalUI
//{
//    bool AskUserApproval(HWND hWnd, uint16_t vid, uint16_t pid, uint64_t fingerprint);
//}

#pragma once
#include <windows.h>
#include <vector>
#include <string>
#include <cstdint>

struct UsbItem {
    uint16_t vid = 0;
    uint16_t pid = 0;
    uint64_t fingerprint = 0;

    std::wstring devicePath; // 원본 path(로그/추적용)
    std::wstring name;       // optional (없으면 빈 문자열)
    bool allow = false;      // 체크됨=Whitelist, 미체크=Blacklist
};

namespace ApprovalUI {
    // devices: UI에서 allow 값을 수정함(체크박스 결과가 반영됨)
    // return: Save 눌렀으면 true, Cancel/닫기면 false
    bool ShowUsbListDialog(HWND parent, std::vector<UsbItem>& devices);
}