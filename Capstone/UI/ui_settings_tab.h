#pragma once

#include <windows.h>

namespace UiSettingsTab
{
    HWND Create(HWND parent, HINSTANCE hInst, const RECT& rect);
    bool HandleCommand(WPARAM wParam, LPARAM lParam);

    // 디스크 상태를 UI로 다시 그린다 (탭 전환/Reload 클릭 시 호출).
    void Refresh();
}
