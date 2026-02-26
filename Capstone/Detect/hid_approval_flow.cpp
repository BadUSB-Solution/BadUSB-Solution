#include "hid_approval_flow.h"

#include <vector>
#include "logger.h"
#include "policy.h"
#include "ui_approval.h"
#include "hid_fingerprint.h"

namespace
{
    bool PendingContains(const std::vector<UsbItem>& pending, uint64_t fp)
    {
        for (const auto& it : pending) {
            if (it.fingerprint == fp) return true;
        }
        return false;
    }

    bool RunApprovalDialog(HWND hWnd, std::vector<UsbItem>& pending)
    {
        if (pending.empty()) return false;

        // UI 표시 전 allow 기본값
        for (auto& it : pending) {
            if (Policy::IsWhitelisted(it.fingerprint)) it.allow = true;
            else if (Policy::IsBlacklisted(it.fingerprint)) it.allow = false;
            else it.allow = false; // 기본 미체크
        }

        Logger::Add(L"[UI] Showing approval list dialog...");
        bool saved = ApprovalUI::ShowUsbListDialog(hWnd, pending);

        if (!saved) {
            Logger::Add(L"[UI] Canceled. No policy changes.");
            return false;
        }

        Logger::Add(L"[UI] Saved. Applying policy...");

        for (const auto& it : pending) {
            if (Policy::IsWhitelisted(it.fingerprint) || Policy::IsBlacklisted(it.fingerprint))
                continue;

            if (it.allow) {
                Policy::AddWhitelist(it.fingerprint);
                Logger::Add(L"[T] Added to Whitelist: " + std::to_wstring(it.fingerprint));
            }
            else {
                Policy::AddBlacklist(it.fingerprint);
                Logger::Add(L"[T] Added to Blacklist: " + std::to_wstring(it.fingerprint));
            }
        }

        // 필요하면 저장
        // Policy::Save();

        return true;
    }
}

bool  DeviceApprovalFlow::ApproveFromPaths(HWND hWnd, const std::vector<std::wstring>& paths)
{
    std::vector<UsbItem> pending;
    pending.reserve(paths.size());

    for (const auto& path : paths) {
        if (path.empty()) continue;

        UsbItem item = HidFingerprint::BuildItem(path);

        // 이미 승인/차단이면 스킵
        if (Policy::IsWhitelisted(item.fingerprint) || Policy::IsBlacklisted(item.fingerprint))
            continue;

        if (!PendingContains(pending, item.fingerprint))
            pending.push_back(std::move(item));
    }

    if (pending.empty()) {
        Logger::Add(L"[Approval] No unknown HID devices.");
        return false;
    }

    return RunApprovalDialog(hWnd, pending);
}

bool  DeviceApprovalFlow::ApproveFromPath(HWND hWnd, const std::wstring& path)
{
    std::vector<std::wstring> one{ path };
    return ApproveFromPaths(hWnd, one);
}