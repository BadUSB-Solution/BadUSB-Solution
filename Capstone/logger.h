#pragma once
#include <windows.h>
#include <string>

namespace Logger
{
    void SetNotifyWindow(HWND hWnd, UINT msgId); // WM_APP_NEWLOG
    void Add(const std::wstring& s);
    std::wstring GetAll(); // CRLF 포함해서 한 번에 반환
}