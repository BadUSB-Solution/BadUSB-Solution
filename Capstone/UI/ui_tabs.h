#pragma once
#include <windows.h>

namespace UiTabs
{
    void Create(HWND hWnd, HINSTANCE hInst);
    void OnSelChange();
    bool HandleCommand(WPARAM wParam, LPARAM lParam);
    void UpdateLogs();
    bool IsLogsTabActive();
}
