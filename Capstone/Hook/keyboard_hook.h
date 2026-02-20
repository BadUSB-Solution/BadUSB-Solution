#pragma once
#include <windows.h>
#include <vector>

namespace KeyboardHook
{
    struct KeyRecord {
        DWORD vkCode;
        DWORD scanCode;
        DWORD flags;
        DWORD time;
    };

    bool Install();
    void Uninstall();
    bool IsActive();
    std::vector<KeyRecord> GetBufferedKeys();
    void ClearBuffer();
}
