#pragma once
#include <windows.h>

namespace HidMonitor
{
    void RegisterNotifications(HWND hWnd);
    LRESULT OnDeviceChange(HWND hWnd, WPARAM wParam, LPARAM lParam); // 처리했으면 0 반환, 아니면 -1
}
