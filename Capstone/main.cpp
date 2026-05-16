// main.cpp : BadUSB Guard Demo (Tray + HID Auto Detect) 껍데기
// 실제로 돌리다보면 로그에 A,B,C,D 이런거 달려있을텐데 해당 과정은 노션에 올려놓은 머메이드 차트 보면 될듯
// 현재 헤더파일 보면 여기에 없는 badusb.h가 있음. 데모에서는 쓰이지 않았지만 기본 드라이버? 구성만 gpt가 잡아준 것.

#include "framework.h"
#include "Capstone.h"

#include <windows.h>
#include <shellapi.h>
#include <commctrl.h>

#include <string>

#include "ui_message_id.h"
#include "logger.h"
#include "ui_tray.h"
#include "ui_tabs.h"
#include "hid_monitor.h"
#include "hid_scan.h"
#include "hid_approval_flow.h"   
#include "keyboard_hook.h"
#include "ui_buffer_view.h"
#include "policy.h"

#pragma comment(lib, "Shell32.lib")

#ifndef MAX_LOADSTRING
#define MAX_LOADSTRING 100
#endif

HINSTANCE hInst;
WCHAR szTitle[MAX_LOADSTRING];
WCHAR szWindowClass[MAX_LOADSTRING];

ATOM MyRegisterClass(HINSTANCE);
BOOL InitInstance(HINSTANCE, int);
LRESULT CALLBACK WndProc(HWND, UINT, WPARAM, LPARAM);

// WinMain
int APIENTRY wWinMain(HINSTANCE hInstance, HINSTANCE, LPWSTR, int)
{
    hInst = hInstance;

    LoadStringW(hInstance, IDS_APP_TITLE, szTitle, MAX_LOADSTRING);
    LoadStringW(hInstance, IDC_CAPSTONE, szWindowClass, MAX_LOADSTRING);

    MyRegisterClass(hInstance);

    // DB 먼저 로드 시도 (InitInstance 내부에서 참조)
    if (Policy::Load()) {
        Logger::Add(L"[A] Policy DB loaded from file.");
    }

    InitInstance(hInstance, 0);
    Logger::Add(L"[A] Start (Tray resident)");

    MSG msg;
    while (GetMessage(&msg, nullptr, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }
    return 0;
}

// Register / Init
ATOM MyRegisterClass(HINSTANCE hInstance)
{
    WNDCLASSEXW wcex{ sizeof(WNDCLASSEXW) };
    wcex.lpfnWndProc = WndProc;
    wcex.hInstance = hInstance;
    wcex.hIcon = LoadIcon(hInstance, MAKEINTRESOURCE(IDI_CAPSTONE));
    wcex.hCursor = LoadCursor(nullptr, IDC_ARROW);
    wcex.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
    wcex.lpszClassName = szWindowClass;
    return RegisterClassExW(&wcex);
}

BOOL InitInstance(HINSTANCE hInstance, int)
{
    HWND hWnd = CreateWindowW(
        szWindowClass, szTitle,
        WS_OVERLAPPEDWINDOW,
        CW_USEDEFAULT, 0, 900, 600,
        nullptr, nullptr, hInstance, nullptr
    );
    if (!hWnd) return FALSE;

    Logger::SetNotifyWindow(hWnd, WM_APP_NEWLOG);

    UiTabs::Create(hWnd, hInst);
    HidMonitor::RegisterNotifications(hWnd);
    UiTray::AddIcon(hWnd, hInst, IDI_CAPSTONE);

    // DB에 기존 정책이 있으면 스캔 생략, 없으면 전체 스캔 → 승인 UI
    if (Policy::GetWhitelist().empty() && Policy::GetBlacklist().empty()) {
        auto paths = HidScan::EnumeratePresentHidDevicePaths();
        DeviceApprovalFlow::ApproveDevicesFromPaths(nullptr, paths);
    }
    else {
        Logger::Add(L"[A] Skipped initial scan (policy DB exists).");
    }

    ShowWindow(hWnd, SW_HIDE);
    return TRUE;
}

// WndProc (Router)
LRESULT CALLBACK WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    switch (msg)
    {
    case WM_APP_NEWLOG:
        if (UiTabs::IsLogsTabActive())
            UiTabs::UpdateLogs();
        return 0;

    case WM_NOTIFY:
        if (((LPNMHDR)lParam)->code == TCN_SELCHANGE) {
            UiTabs::OnSelChange();
        }
        return 0;

    case WM_DEVICECHANGE:
    {
        LRESULT r = HidMonitor::OnDeviceChange(hWnd, wParam, lParam);
        if (r == 0) return 0; // 처리됨
        break;
    }

    case WM_TRAYICON:
        if (lParam == WM_RBUTTONUP) UiTray::ShowMenu(hWnd);
        if (lParam == WM_LBUTTONDBLCLK) {
            ShowWindow(hWnd, SW_SHOW);
            SetForegroundWindow(hWnd);
        }
        return 0;

    case WM_COMMAND:
        if (UiTabs::HandleCommand(wParam, lParam)) {
            return 0;
        }
        if (LOWORD(wParam) == IDM_TRAY_OPEN) {
            ShowWindow(hWnd, SW_SHOW);
            SetForegroundWindow(hWnd);
        }
        if (LOWORD(wParam) == IDM_TRAY_EXIT) {
            DestroyWindow(hWnd);
        }
        return 0;

    case WM_CLOSE:
        ShowWindow(hWnd, SW_HIDE);
        return 0;

    case WM_DESTROY:
        UiTray::RemoveIcon(hWnd);
        PostQuitMessage(0);
        return 0;
    }

    return DefWindowProc(hWnd, msg, wParam, lParam);
}
