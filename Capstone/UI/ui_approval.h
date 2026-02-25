#pragma once
#include <windows.h>
#include <vector>
#include <string>
#include <cstdint>

#include "../Core/usb_item.h"

namespace ApprovalUI {
    // devices: UI에서 allow 값을 수정함(체크박스 결과가 반영됨)
    // return: Save 눌렀으면 true, Cancel/닫기면 false
    bool ShowUsbListDialog(HWND parent, std::vector<UsbItem>& devices);
}