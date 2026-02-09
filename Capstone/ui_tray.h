#pragma once
#include <windows.h>

namespace UiTray
{
    void AddIcon(HWND hWnd, HINSTANCE hInst, int iconResId);
    void RemoveIcon(HWND hWnd);
    void ShowMenu(HWND hWnd);
}
