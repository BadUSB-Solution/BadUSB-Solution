#pragma once

#include <windows.h>

namespace UiPolicyTab
{
    HWND Create(HWND parent, HINSTANCE hInst, const RECT& rect);
    bool HandleCommand(WPARAM wParam, LPARAM lParam);
    void Refresh();
}
