// 사용자 환경설정 UI 탭.
// - 체크박스/라디오로 Policy::Settings 의 모든 필드를 노출.
// - Save 클릭 시 Policy::SetSettings 로 디스크 반영, Reload 는 디스크에서 다시 읽기.

#include "ui_settings_tab.h"

#include <commctrl.h>
#include <string>

#include "logger.h"
#include "settings.h"

#pragma comment(lib, "Comctl32.lib")

namespace
{
    // 컨트롤 ID — Policy 탭(41xxx)과 안 겹치게 42xxx 대역.
    constexpr int IDC_SET_AUTO_START       = 42001;
    constexpr int IDC_SET_MINIMIZE_TRAY    = 42002;

    constexpr int IDC_SET_UNK_APPROVAL     = 42010;
    constexpr int IDC_SET_UNK_AUTO_BLOCK   = 42011;
    constexpr int IDC_SET_UNK_AUTO_ALLOW   = 42012;

    constexpr int IDC_SET_BEHAVIOR_MONITOR = 42020;
    constexpr int IDC_SET_NOTIFY_RISK      = 42030;

    constexpr int IDC_SETTINGS_SAVE        = 42100;
    constexpr int IDC_SETTINGS_RELOAD      = 42101;

    HWND g_root            = nullptr;
    HWND g_chkAutoStart    = nullptr;
    HWND g_chkMinTray      = nullptr;
    HWND g_radApproval     = nullptr;
    HWND g_radAutoBlock    = nullptr;
    HWND g_radAutoAllow    = nullptr;
    HWND g_chkBehavior     = nullptr;
    HWND g_chkNotify       = nullptr;
    HWND g_btnSave         = nullptr;
    HWND g_btnReload       = nullptr;
    WNDPROC g_prevRootProc = nullptr;

    HMENU ControlId(int id)
    {
        return reinterpret_cast<HMENU>(static_cast<INT_PTR>(id));
    }

    HWND MakeStatic(HWND parent, HINSTANCE hInst, LPCWSTR text, int x, int y, int w, int h)
    {
        return CreateWindowW(
            L"STATIC", text,
            WS_CHILD | WS_VISIBLE,
            x, y, w, h,
            parent, nullptr, hInst, nullptr);
    }

    HWND MakeCheckBox(HWND parent, HINSTANCE hInst, LPCWSTR text, int id, int x, int y, int w, int h)
    {
        return CreateWindowW(
            L"BUTTON", text,
            WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_AUTOCHECKBOX,
            x, y, w, h,
            parent, ControlId(id), hInst, nullptr);
    }

    HWND MakeRadio(HWND parent, HINSTANCE hInst, LPCWSTR text, int id, int x, int y, int w, int h, bool firstInGroup)
    {
        DWORD style = WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_AUTORADIOBUTTON;
        if (firstInGroup) style |= WS_GROUP;
        return CreateWindowW(
            L"BUTTON", text,
            style,
            x, y, w, h,
            parent, ControlId(id), hInst, nullptr);
    }

    HWND MakeButton(HWND parent, HINSTANCE hInst, LPCWSTR text, int id, int x, int y, int w, int h)
    {
        return CreateWindowW(
            L"BUTTON", text,
            WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_PUSHBUTTON,
            x, y, w, h,
            parent, ControlId(id), hInst, nullptr);
    }

    void SetChecked(HWND hCtl, bool checked)
    {
        SendMessageW(hCtl, BM_SETCHECK, checked ? BST_CHECKED : BST_UNCHECKED, 0);
    }

    bool IsChecked(HWND hCtl)
    {
        return SendMessageW(hCtl, BM_GETCHECK, 0, 0) == BST_CHECKED;
    }

    // UI → struct.
    Policy::Settings ReadFromUi()
    {
        Policy::Settings s;
        s.autoStart          = IsChecked(g_chkAutoStart);
        s.minimizeToTray     = IsChecked(g_chkMinTray);
        s.behaviorMonitoring = IsChecked(g_chkBehavior);
        s.notifyOnRisk       = IsChecked(g_chkNotify);

        if (IsChecked(g_radAutoBlock))      s.unknownDeviceAction = Policy::UnknownDeviceAction::AutoBlock;
        else if (IsChecked(g_radAutoAllow)) s.unknownDeviceAction = Policy::UnknownDeviceAction::AutoAllow;
        else                                s.unknownDeviceAction = Policy::UnknownDeviceAction::Approval;

        return s;
    }

    // struct → UI.
    void WriteToUi(const Policy::Settings& s)
    {
        SetChecked(g_chkAutoStart, s.autoStart);
        SetChecked(g_chkMinTray,   s.minimizeToTray);
        SetChecked(g_chkBehavior,  s.behaviorMonitoring);
        SetChecked(g_chkNotify,    s.notifyOnRisk);

        SetChecked(g_radApproval,  s.unknownDeviceAction == Policy::UnknownDeviceAction::Approval);
        SetChecked(g_radAutoBlock, s.unknownDeviceAction == Policy::UnknownDeviceAction::AutoBlock);
        SetChecked(g_radAutoAllow, s.unknownDeviceAction == Policy::UnknownDeviceAction::AutoAllow);
    }

    LRESULT CALLBACK RootProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam)
    {
        if (msg == WM_COMMAND && UiSettingsTab::HandleCommand(wParam, lParam)) {
            return 0;
        }
        return CallWindowProcW(g_prevRootProc, hWnd, msg, wParam, lParam);
    }
}

namespace UiSettingsTab
{
    HWND Create(HWND parent, HINSTANCE hInst, const RECT& rect)
    {
        g_root = CreateWindowW(
            L"STATIC", L"",
            WS_CHILD | WS_VISIBLE,
            rect.left, rect.top,
            rect.right - rect.left,
            rect.bottom - rect.top,
            parent, nullptr, hInst, nullptr);

        g_prevRootProc = reinterpret_cast<WNDPROC>(
            SetWindowLongPtrW(g_root, GWLP_WNDPROC, reinterpret_cast<LONG_PTR>(RootProc)));

        const int contentW = rect.right - rect.left;

        // ── 1. 일반 환경설정 ──
        MakeStatic(g_root, hInst, L"일반 환경설정",
                   12, 10, 200, 20);
        g_chkAutoStart = MakeCheckBox(g_root, hInst, L"Windows 시작 시 자동으로 실행",
                                      IDC_SET_AUTO_START, 24, 36, 360, 22);
        g_chkMinTray = MakeCheckBox(g_root, hInst, L"창을 닫을 때 시스템 트레이로 최소화",
                                    IDC_SET_MINIMIZE_TRAY, 24, 62, 360, 22);

        // ── 2. 위험 탐지 정책 ──
        MakeStatic(g_root, hInst, L"위험 탐지 정책",
                   12, 102, 200, 20);
        MakeStatic(g_root, hInst, L"새로운 미등록 장치 연결 시 처리:",
                   24, 128, 360, 20);
        g_radApproval  = MakeRadio(g_root, hInst, L"승인 대기",
                                   IDC_SET_UNK_APPROVAL,   40, 152, 160, 22, true);
        g_radAutoBlock = MakeRadio(g_root, hInst, L"자동 차단",
                                   IDC_SET_UNK_AUTO_BLOCK, 40, 178, 160, 22, false);
        g_radAutoAllow = MakeRadio(g_root, hInst, L"자동 허용",
                                   IDC_SET_UNK_AUTO_ALLOW, 40, 204, 160, 22, false);

        g_chkBehavior = MakeCheckBox(g_root, hInst, L"행위 기반 모니터링 활성화",
                                     IDC_SET_BEHAVIOR_MONITOR, 24, 236, 360, 22);

        // ── 3. 알림 및 리포트 ──
        MakeStatic(g_root, hInst, L"알림 및 리포트",
                   12, 278, 200, 20);
        g_chkNotify = MakeCheckBox(g_root, hInst, L"위험 장치 연결 시 우측 하단 팝업 알림 표시",
                                   IDC_SET_NOTIFY_RISK, 24, 304, 400, 22);

        // ── 액션 버튼 ──
        g_btnSave   = MakeButton(g_root, hInst, L"Save",   IDC_SETTINGS_SAVE,   12, 348, 100, 30);
        g_btnReload = MakeButton(g_root, hInst, L"Reload", IDC_SETTINGS_RELOAD, 120, 348, 100, 30);

        Refresh();
        (void)contentW;
        return g_root;
    }

    bool HandleCommand(WPARAM wParam, LPARAM)
    {
        switch (LOWORD(wParam)) {
        case IDC_SETTINGS_SAVE: {
            const Policy::Settings s = ReadFromUi();
            const bool ok = Policy::SetSettings(s);
            Logger::Add(ok
                ? L"[SettingsUI] Saved settings to disk."
                : L"[SettingsUI] Save failed.");
            return true;
        }

        case IDC_SETTINGS_RELOAD: {
            const bool ok = Policy::LoadSettings();
            Refresh();
            Logger::Add(ok
                ? L"[SettingsUI] Reloaded settings from disk."
                : L"[SettingsUI] Reload failed.");
            return true;
        }

        // 체크박스/라디오 클릭은 BS_AUTO* 가 알아서 처리 — 별도 핸들링 불필요.
        case IDC_SET_AUTO_START:
        case IDC_SET_MINIMIZE_TRAY:
        case IDC_SET_UNK_APPROVAL:
        case IDC_SET_UNK_AUTO_BLOCK:
        case IDC_SET_UNK_AUTO_ALLOW:
        case IDC_SET_BEHAVIOR_MONITOR:
        case IDC_SET_NOTIFY_RISK:
            return true;

        default:
            return false;
        }
    }

    void Refresh()
    {
        if (!g_root) return;
        WriteToUi(Policy::GetSettings());
    }
}
