// usb 허용 ui 부분

#include "ui_approval.h"
#include <sstream>
#include <string>

static std::wstring HexU16(uint16_t v)
{
    std::wstringstream ss;
    ss << L"0x" << std::hex << v << std::dec;
    return ss.str();
}

namespace ApprovalUI
{
    bool AskUserApproval(HWND hWnd, uint16_t vid, uint16_t pid, uint64_t fingerprint)
    {
        std::wstringstream ss;
        ss << L"Unknown HID device detected.\n\n"
            << L"VID: " << HexU16(vid) << L"\n"
            << L"PID: " << HexU16(pid) << L"\n"
            << L"Fingerprint: " << fingerprint << L"\n\n"
            << L"Allow this device?";

        int r = MessageBoxW(
            hWnd,
            ss.str().c_str(),
            L"BadUSB Guard",
            MB_ICONWARNING | MB_YESNO | MB_DEFBUTTON2
        );
        return (r == IDYES);
    }
}
