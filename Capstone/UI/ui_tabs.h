#pragma once
#include <windows.h>

namespace UiTabs
{
    void Create(HWND hWnd, HINSTANCE hInst);
    void OnSelChange();
    void UpdateLogs(); // logger에서 문자열 가져와 Edit에 반영
    bool IsLogsTabActive();
}
