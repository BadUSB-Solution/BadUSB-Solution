#include "hid_approval_flow.h"

#include <algorithm>
#include <cstdint>
#include <cwchar>
#include <vector>

#include "hid_fingerprint.h"
#include "hid_scan.h"
#include "keyboard_hook.h"
#include "logger.h"
#include "json_log.h"
#include "policy.h"
#include "../Policy/device_blocker.h"
#include "ui_approval.h"
#include "ui_buffer_view.h"

namespace
{
    // 중복 fingerprint 후보 포함 여부 확인
    bool ContainsPendingFingerprint(const std::vector<UsbItem>& pending, uint64_t fp)
    {
        for (const auto& item : pending) {
            if (item.fingerprint == fp) {
                return true;
            }
        }
        return false;
    }

    // 후보 목록 정렬: VID -> PID -> Name -> Fingerprint
    void SortPending(std::vector<UsbItem>& pending)
    {
        std::sort(pending.begin(), pending.end(), [](const UsbItem& left, const UsbItem& right) {
            if (left.vid != right.vid) {
                return left.vid < right.vid;
            }
            if (left.pid != right.pid) {
                return left.pid < right.pid;
            }

            const int nameCmp = _wcsicmp(left.name.c_str(), right.name.c_str());
            if (nameCmp != 0) {
                return nameCmp < 0;
            }

            return left.fingerprint < right.fingerprint;
        });
    }

    bool RunApprovalDialog(HWND hWnd, std::vector<UsbItem>& pending)
    {
        if (pending.empty()) {
            return false;
        }

        // UI 초기 체크 상태 설정:
        // 기존 정책이 있으면 체크 상태에 반영하고, 새 장치는 기본 차단 상태로 둔다.
        for (auto& item : pending) {
            const Policy::PolicyStatus status = Policy::Evaluate(item);
            if (status == Policy::PolicyStatus::Blacklisted) {
                item.allow = false;
            }
            else if (status == Policy::PolicyStatus::Whitelisted) {
                item.allow = true;
            }
            else {
                item.allow = false;
            }
        }

        Logger::Add(L"[UI] Showing approval list dialog...");
        const bool saved = ApprovalUI::ShowUsbListDialog(hWnd, pending);

        if (!saved) {
            Logger::Add(L"[UI] Canceled. No policy changes.");
            return false;
        }

        Logger::Add(L"[UI] Saved. Applying policy...");

        // 항목별 정책 반영.
        // blacklist로 저장한 항목은 현재 연결된 장치도 즉시 disable/eject 시도한다.
        for (const auto& item : pending) {
            if (item.allow) {
                Policy::AddWhitelist(item);
                Logger::Add(L"[T] Saved as Whitelist: " + std::to_wstring(item.fingerprint));
                std::wstring enableDetail;
                const DeviceBlocker::EnableResult enableResult =
                    DeviceBlocker::EnableByVidPid(item.vid, item.pid, &enableDetail);
                Logger::Add(L"[Block] Saved whitelist item enable result=" +
                    std::to_wstring(static_cast<int>(enableResult)) + L", " + enableDetail);
            }
            else {
                Policy::AddBlacklist(item);
                Logger::Add(L"[T] Saved as Blacklist: " + std::to_wstring(item.fingerprint));
                if (!item.devicePath.empty()) {
                    std::wstring blockDetail;
                    const DeviceBlocker::BlockResult blockResult =
                        DeviceBlocker::BlockByDevicePath(item.devicePath, &blockDetail);
                    Logger::Add(L"[Block] Saved blacklist item block result=" +
                        std::to_wstring(static_cast<int>(blockResult)) + L", " + blockDetail);
                }
            }
        }

        return true;
    }
}

bool DeviceApprovalFlow::ApproveDevicesFromPaths(HWND hWnd, const std::vector<std::wstring>& paths)
{
    std::vector<UsbItem> pending;
    pending.reserve(paths.size());

    for (const auto& path : paths) {
        if (path.empty()) {
            continue;
        }

        UsbItem item = HidFingerprint::BuildUsbItemFromPath(path);
        const Policy::PolicyStatus status = Policy::Evaluate(item);

        // 이미 차단된 후보는 승인 UI에 다시 올리지 않는다.
        if (status == Policy::PolicyStatus::Blacklisted) {
            Logger::Add(L"[Approval] Auto-block candidate (blacklisted VID/PID): " + item.name);
            continue;
        }

        // whitelist 미포함 fingerprint 후보 규칙
        if (status == Policy::PolicyStatus::Whitelisted) {
            continue;
        }

        if (ContainsPendingFingerprint(pending, item.fingerprint)) {
            continue;
        }

        pending.push_back(std::move(item));
    }

    if (pending.empty()) {
        Logger::Add(L"[Approval] No unknown HID devices.");
        return false;
    }

    SortPending(pending);

    for (const auto& item : pending) {
        Logger::Add(L"[Approval] Candidate: " + item.name);
    }

    return RunApprovalDialog(hWnd, pending);
}

bool DeviceApprovalFlow::ApproveDeviceFromPath(HWND hWnd, const std::wstring& path)
{
    if (path.empty()) return false;

    UsbItem hotplugItem = HidFingerprint::BuildUsbItemFromPath(path);
    const Policy::PolicyStatus status = Policy::Evaluate(hotplugItem);

    // 이미 차단된 장치 -> 승인 UI 없이 Windows 장치 비활성화/eject 시도
    if (status == Policy::PolicyStatus::Blacklisted) {
        Logger::Add(L"[Approval] Auto-block (blacklisted): " + hotplugItem.name);
        std::wstring blockDetail;
        const DeviceBlocker::BlockResult blockResult = DeviceBlocker::BlockByDevicePath(path, &blockDetail);
        Logger::Add(L"[Block] Result=" + std::to_wstring(static_cast<int>(blockResult)) + L", " + blockDetail);
        JsonLog::Add(hotplugItem.name, L"DEVICE_BLOCKED",
                     L"result=" + std::to_wstring(static_cast<int>(blockResult)) + L" " + blockDetail,
                     -1, L"DANGER");
        return false;
    }

    // 이미 허용된 장치 -> Hook/뷰어/다이얼로그 없이 통과
    if (status == Policy::PolicyStatus::Whitelisted) {
        Logger::Add(L"[Approval] Auto-allow (whitelisted): " + hotplugItem.name);
        JsonLog::Add(hotplugItem.name, L"DEVICE_ALLOWED", L"whitelist match",
                     -1, L"SAFE");
        return true;
    }

    // ── Unknown → 키보드 훅 + 모니터링 창 + 승인 다이얼로그 동시 ──
    Logger::Add(L"[Approval] Unknown device → starting hook + viewer: " + hotplugItem.name);

    KeyboardHook::Install();
    BufferViewUI::Open(hWnd, hotplugItem.vid, hotplugItem.pid, hotplugItem.name);

    // 승인 리스트 다이얼로그 (모니터링 창과 동시에 표시)
    auto paths = HidScan::EnumeratePresentHidDevicePaths();

    // 열거 지연 대비 도착 경로 강제 포함
    {
        bool exists = false;
        for (const auto& p : paths) {
            if (_wcsicmp(p.c_str(), path.c_str()) == 0) {
                exists = true;
                break;
            }
        }
        if (!exists) paths.push_back(path);
    }

    // 후보 목록 빌드 → 승인 다이얼로그 표시
    bool dialogResult = ApproveDevicesFromPaths(hWnd, paths);

    // 다이얼로그 닫힘 → 모니터링 창 + 훅 해제
    BufferViewUI::Close();
    KeyboardHook::Uninstall();

    return dialogResult;
}
