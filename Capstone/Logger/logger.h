#pragma once
#include <windows.h>
#include <string>

namespace Logger
{
    void SetNotifyWindow(HWND hWnd, UINT msgId); // WM_APP_NEWLOG
    void Add(const std::wstring& s);
    void Add(const wchar_t* s);
    std::wstring GetAll(); // CRLF 포함해서 한 번에 반환

    // ─── 의미 함수 (호출처에 문자열 노출 X) ───────────────────────
    // 모든 메시지 문자열은 logger.cpp 본문에서 AY_OBFUSCATE 로 컴파일타임 암호화.
    // 호출처는 함수 이름만 호출하므로 분석자가 호출 측에서 문자열 단서를 얻지 못함.
    namespace Msg
    {
        // App lifecycle
        void AppSettingsLoaded();
        void AppPolicyDbLoaded();
        void AppStart();
        void AppSkippedInitialScan();

        // Init (HID notification)
        void InitHidRegistered(bool ok);

        // Hook
        void HookInstalled();
        void HookInstallFailed();
        void HookRemoved();

        // Auto (HID monitor)
        void AutoArrivalDuplicate();
        void AutoArrival();
        void AutoRemoved();

        // Approval
        void ApprovalAutoBlockCandidateVidPid(const std::wstring& name);
        void ApprovalNoUnknownDevices();
        void ApprovalCandidate(const std::wstring& name);
        void ApprovalAutoBlock(const std::wstring& name);
        void ApprovalAutoAllow(const std::wstring& name);
        void ApprovalUnknownStartHook(const std::wstring& name);

        // UI (approval dialog)
        void ShowingApprovalDialog();
        void Canceled();
        void Saved();

        // State (정책에 저장됨)
        void SavedAsWhitelist(unsigned long long fingerprint);
        void SavedAsBlacklist(unsigned long long fingerprint);

        // Block
        void BlockSavedWhitelistEnableResult(int result, const std::wstring& detail);
        void BlockSavedBlacklistResult(int result, const std::wstring& detail);
        void BlockResult(int result, const std::wstring& detail);
        void BlockDisabledWithId(const std::wstring& instanceId);
        void BlockDisabled();
        void BlockEjectedWithId(const std::wstring& instanceId);
        void BlockFailed();
        void BlockReEnabled(const std::wstring& instanceId);
        void BlockRemovedForRescan(const std::wstring& instanceId);

        // PolicyUI
        void PolicyNoSelection();
        void PolicyAllowedSelected(const std::wstring& name);
        void PolicyEnableResult(int result, const std::wstring& detail);

        // SettingsUI
        void SettingsSaveResult(bool ok);
        void SettingsReloadResult(bool ok);

        // Score (level enum: 0=SAFE, 1=NORMAL, 2=WARN, 3=DANGER, else=UNKNOWN)
        void ScoreFinal(double final_score, int level);
    }
}
