#pragma once
#include <windows.h>
#include <cstdint>

namespace ApprovalUI
{
    bool AskUserApproval(HWND hWnd, uint16_t vid, uint16_t pid, uint64_t fingerprint);
}
