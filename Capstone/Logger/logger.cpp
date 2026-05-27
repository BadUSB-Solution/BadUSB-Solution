// 앱 상에 로그 띄우는 부분

#include "logger.h"
#include "obfuscate.h"
#include <vector>
#include <mutex>

namespace Logger
{
    static std::mutex g_mu;
    static std::vector<std::wstring> g_logs;

    static HWND g_notifyWnd = nullptr;
    static UINT g_notifyMsg = 0;

    void SetNotifyWindow(HWND hWnd, UINT msgId)
    {
        g_notifyWnd = hWnd;
        g_notifyMsg = msgId;
    }

    void Add(const std::wstring& s)
    {
        {
            std::lock_guard<std::mutex> lk(g_mu);
            g_logs.push_back(s);
            if (g_logs.size() > 500) g_logs.erase(g_logs.begin());
        }
        if (g_notifyWnd && g_notifyMsg)
            PostMessage(g_notifyWnd, g_notifyMsg, 0, 0);
    }

    void Add(const wchar_t* s)
    {
        Add(std::wstring(s ? s : L""));
    }

    std::wstring GetAll()
    {
        std::wstring all;
        std::lock_guard<std::mutex> lk(g_mu);
        for (auto& s : g_logs) {
            all += s + L"\r\n";
        }
        return all;
    }

    // ─── Msg: 의미 함수들 ──────────────────────────────────────────
    // noinline 으로 호출처에 인라인되지 않게 강제 — 디크립트 코드/문자열은 logger.cpp 본문에만 존재.
    namespace Msg
    {
        // App lifecycle
        __declspec(noinline) void AppSettingsLoaded() {
            Add(AY_OBFUSCATE(L"[A] Settings loaded."));
        }
        __declspec(noinline) void AppPolicyDbLoaded() {
            Add(AY_OBFUSCATE(L"[A] Policy DB loaded from file."));
        }
        __declspec(noinline) void AppStart() {
            Add(AY_OBFUSCATE(L"[A] Start (Tray resident)"));
        }
        __declspec(noinline) void AppSkippedInitialScan() {
            Add(AY_OBFUSCATE(L"[A] Skipped initial scan (policy DB exists)."));
        }

        // Init
        __declspec(noinline) void InitHidRegistered(bool ok) {
            Add(ok
                ? std::wstring(AY_OBFUSCATE(L"[Init] HID notifications registered."))
                : std::wstring(AY_OBFUSCATE(L"[Init] HID notification registration failed.")));
        }

        // Hook
        __declspec(noinline) void HookInstalled() {
            Add(AY_OBFUSCATE(L"[Hook] Keyboard hook installed. All keyboard input blocked."));
        }
        __declspec(noinline) void HookInstallFailed() {
            Add(AY_OBFUSCATE(L"[Hook] Keyboard hook install FAILED."));
        }
        __declspec(noinline) void HookRemoved() {
            Add(AY_OBFUSCATE(L"[Hook] Keyboard hook removed. Keyboard restored."));
        }

        // Auto
        __declspec(noinline) void AutoArrivalDuplicate() {
            Add(AY_OBFUSCATE(L"[Auto] Duplicate arrival ignored (same DevicePath)."));
        }
        __declspec(noinline) void AutoArrival() {
            Add(AY_OBFUSCATE(L"[Auto] HID arrival detected."));
        }
        __declspec(noinline) void AutoRemoved() {
            Add(AY_OBFUSCATE(L"[Auto] HID removed."));
        }

        // Approval
        __declspec(noinline) void ApprovalAutoBlockCandidateVidPid(const std::wstring& name) {
            Add(std::wstring(AY_OBFUSCATE(L"[Approval] Auto-block candidate (blacklisted VID/PID): ")) + name);
        }
        __declspec(noinline) void ApprovalNoUnknownDevices() {
            Add(AY_OBFUSCATE(L"[Approval] No unknown HID devices."));
        }
        __declspec(noinline) void ApprovalCandidate(const std::wstring& name) {
            Add(std::wstring(AY_OBFUSCATE(L"[Approval] Candidate: ")) + name);
        }
        __declspec(noinline) void ApprovalAutoBlock(const std::wstring& name) {
            Add(std::wstring(AY_OBFUSCATE(L"[Approval] Auto-block (blacklisted): ")) + name);
        }
        __declspec(noinline) void ApprovalAutoAllow(const std::wstring& name) {
            Add(std::wstring(AY_OBFUSCATE(L"[Approval] Auto-allow (whitelisted): ")) + name);
        }
        __declspec(noinline) void ApprovalUnknownStartHook(const std::wstring& name) {
            Add(std::wstring(AY_OBFUSCATE(L"[Approval] Unknown device → starting hook + viewer: ")) + name);
        }

        // UI
        __declspec(noinline) void ShowingApprovalDialog() {
            Add(AY_OBFUSCATE(L"[UI] Showing approval list dialog..."));
        }
        __declspec(noinline) void Canceled() {
            Add(AY_OBFUSCATE(L"[UI] Canceled. No policy changes."));
        }
        __declspec(noinline) void Saved() {
            Add(AY_OBFUSCATE(L"[UI] Saved. Applying policy..."));
        }

        // State
        __declspec(noinline) void SavedAsWhitelist(unsigned long long fingerprint) {
            Add(std::wstring(AY_OBFUSCATE(L"[State] Saved as Whitelist: ")) + std::to_wstring(fingerprint));
        }
        __declspec(noinline) void SavedAsBlacklist(unsigned long long fingerprint) {
            Add(std::wstring(AY_OBFUSCATE(L"[State] Saved as Blacklist: ")) + std::to_wstring(fingerprint));
        }

        // Block
        __declspec(noinline) void BlockSavedWhitelistEnableResult(int result, const std::wstring& detail) {
            Add(std::wstring(AY_OBFUSCATE(L"[Block] Saved whitelist item enable result=")) + std::to_wstring(result) + std::wstring(AY_OBFUSCATE(L", ")) + detail);
        }
        __declspec(noinline) void BlockSavedBlacklistResult(int result, const std::wstring& detail) {
            Add(std::wstring(AY_OBFUSCATE(L"[Block] Saved blacklist item block result=")) + std::to_wstring(result) + std::wstring(AY_OBFUSCATE(L", ")) + detail);
        }
        __declspec(noinline) void BlockResult(int result, const std::wstring& detail) {
            Add(std::wstring(AY_OBFUSCATE(L"[Block] Result=")) + std::to_wstring(result) + std::wstring(AY_OBFUSCATE(L", ")) + detail);
        }
        __declspec(noinline) void BlockDisabledWithId(const std::wstring& instanceId) {
            Add(std::wstring(AY_OBFUSCATE(L"[Block] Blacklisted HID disabled: ")) + instanceId);
        }
        __declspec(noinline) void BlockDisabled() {
            Add(AY_OBFUSCATE(L"[Block] Blacklisted HID disabled."));
        }
        __declspec(noinline) void BlockEjectedWithId(const std::wstring& instanceId) {
            Add(std::wstring(AY_OBFUSCATE(L"[Block] Blacklisted HID ejected: ")) + instanceId);
        }
        __declspec(noinline) void BlockFailed() {
            Add(AY_OBFUSCATE(L"[Block] Failed to disable/eject blacklisted HID."));
        }
        __declspec(noinline) void BlockReEnabled(const std::wstring& instanceId) {
            Add(std::wstring(AY_OBFUSCATE(L"[Block] Re-enabled device: ")) + instanceId);
        }
        __declspec(noinline) void BlockRemovedForRescan(const std::wstring& instanceId) {
            Add(std::wstring(AY_OBFUSCATE(L"[Block] Removed disabled device for rescan: ")) + instanceId);
        }

        // PolicyUI
        __declspec(noinline) void PolicyNoSelection() {
            Add(AY_OBFUSCATE(L"[Policy] No policy item selected."));
        }
        __declspec(noinline) void PolicyAllowedSelected(const std::wstring& name) {
            Add(std::wstring(AY_OBFUSCATE(L"[Policy] Allowed selected item: ")) + name);
        }
        __declspec(noinline) void PolicyEnableResult(int result, const std::wstring& detail) {
            Add(std::wstring(AY_OBFUSCATE(L"[Policy] Enable result=")) + std::to_wstring(result) + std::wstring(AY_OBFUSCATE(L", ")) + detail);
        }

        // SettingsUI
        __declspec(noinline) void SettingsSaveResult(bool ok) {
            Add(ok
                ? std::wstring(AY_OBFUSCATE(L"[Settings] Saved settings to disk."))
                : std::wstring(AY_OBFUSCATE(L"[Settings] Save failed.")));
        }
        __declspec(noinline) void SettingsReloadResult(bool ok) {
            Add(ok
                ? std::wstring(AY_OBFUSCATE(L"[Settings] Reloaded settings from disk."))
                : std::wstring(AY_OBFUSCATE(L"[Settings] Reload failed.")));
        }

        // Score
        __declspec(noinline) void ScoreFinal(double final_score, int level) {
            const wchar_t* lvName;
            switch (level) {
                case 0: lvName = AY_OBFUSCATE(L"SAFE");    break;
                case 1: lvName = AY_OBFUSCATE(L"NORMAL");  break;
                case 2: lvName = AY_OBFUSCATE(L"WARN");    break;
                case 3: lvName = AY_OBFUSCATE(L"DANGER");  break;
                default: lvName = AY_OBFUSCATE(L"UNKNOWN"); break;
            }
            wchar_t buf[128];
            _snwprintf_s(buf, _TRUNCATE, AY_OBFUSCATE(L"[Score] final=%.2f, %ls"), final_score, lvName);
            Add(buf);
        }
    }
}
