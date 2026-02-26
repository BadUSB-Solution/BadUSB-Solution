#include "hid_approval_flow.h"

#include <algorithm>
#include <cstdint>
#include <cwchar>
#include <unordered_map>
#include <vector>

#include "hid_fingerprint.h"
#include "hid_initialScan.h"
#include "logger.h"
#include "policy.h"
#include "ui_approval.h"

namespace
{
    // 동일 VID/PID 다중 fingerprint 묶음 표현 구조
    struct PendingGroup {
        UsbItem displayItem;
        std::vector<uint64_t> memberFingerprints;
    };

    uint64_t MakeGroupKey(const UsbItem& item)
    {
        // VID/PID 우선 그룹 키 생성
        if (item.vid != 0 || item.pid != 0) {
            return (1ULL << 63) | (static_cast<uint64_t>(item.vid) << 16) | static_cast<uint64_t>(item.pid);
        }
        return item.fingerprint;
    }

    bool ContainsFingerprint(const std::vector<uint64_t>& values, uint64_t fp)
    {
        for (uint64_t value : values) {
            if (value == fp) {
                return true;
            }
        }
        return false;
    }

    void SortGroups(std::vector<PendingGroup>& groups)
    {
        std::sort(groups.begin(), groups.end(), [](const PendingGroup& left, const PendingGroup& right) {
            if (left.displayItem.vid != right.displayItem.vid) {
                return left.displayItem.vid < right.displayItem.vid;
            }
            if (left.displayItem.pid != right.displayItem.pid) {
                return left.displayItem.pid < right.displayItem.pid;
            }

            const int nameCmp = _wcsicmp(left.displayItem.name.c_str(), right.displayItem.name.c_str());
            if (nameCmp != 0) {
                return nameCmp < 0;
            }

            return left.displayItem.fingerprint < right.displayItem.fingerprint;
        });
    }

    void FinalizeGroupDisplay(std::vector<PendingGroup>& groups)
    {
        // 병합 개수 [xN] 접미사 부여
        for (auto& group : groups) {
            if (group.memberFingerprints.size() > 1) {
                group.displayItem.name += L" [x" + std::to_wstring(group.memberFingerprints.size()) + L"]";
            }
        }
    }

    void AddOrUpdateGroup(
        std::vector<PendingGroup>& groups,
        std::unordered_map<uint64_t, size_t>& groupIndexByKey,
        UsbItem&& item)
    {
        const uint64_t groupKey = MakeGroupKey(item);
        auto it = groupIndexByKey.find(groupKey);

        if (it == groupIndexByKey.end()) {
            PendingGroup group;
            group.displayItem = std::move(item);
            group.memberFingerprints.push_back(group.displayItem.fingerprint);

            // VID/PID 그룹 fingerprint 표시값 재구성
            if (group.displayItem.vid != 0 || group.displayItem.pid != 0) {
                group.displayItem.fingerprint = (static_cast<uint64_t>(group.displayItem.vid) << 16) |
                    static_cast<uint64_t>(group.displayItem.pid);
            }

            groupIndexByKey.emplace(groupKey, groups.size());
            groups.push_back(std::move(group));
            return;
        }

        PendingGroup& group = groups[it->second];

        if (!ContainsFingerprint(group.memberFingerprints, item.fingerprint)) {
            group.memberFingerprints.push_back(item.fingerprint);
        }

        // Unknown 표시명 대체 우선 적용
        if (group.displayItem.name == L"Unknown HID Device" && item.name != L"Unknown HID Device") {
            group.displayItem.name = item.name;
        }
    }

    bool RunApprovalDialog(HWND hWnd, std::vector<PendingGroup>& groups)
    {
        if (groups.empty()) {
            return false;
        }

        std::vector<UsbItem> pending;
        pending.reserve(groups.size());

        // UI 표시 모델 및 초기 체크 상태 구성
        for (const auto& group : groups) {
            UsbItem item = group.displayItem;
            item.allow = false;

            for (uint64_t fp : group.memberFingerprints) {
                if (Policy::IsWhitelisted(fp)) {
                    item.allow = true;
                    break;
                }
                if (Policy::IsBlacklisted(fp)) {
                    item.allow = false;
                    break;
                }
            }

            pending.push_back(std::move(item));
        }

        Logger::Add(L"[UI] Showing approval list dialog...");
        const bool saved = ApprovalUI::ShowUsbListDialog(hWnd, pending);

        if (!saved) {
            Logger::Add(L"[UI] Canceled. No policy changes.");
            return false;
        }

        Logger::Add(L"[UI] Saved. Applying policy...");

        // 그룹 내 모든 fingerprint 대상 정책 반영
        for (size_t i = 0; i < groups.size(); i++) {
            const bool allow = pending[i].allow;

            for (uint64_t fp : groups[i].memberFingerprints) {
                if (allow) {
                    if (!Policy::IsWhitelisted(fp)) {
                        Policy::AddWhitelist(fp);
                        Logger::Add(L"[T] Added to Whitelist: " + std::to_wstring(fp));
                    }
                }
                else {
                    if (!Policy::IsBlacklisted(fp)) {
                        Policy::AddBlacklist(fp);
                        Logger::Add(L"[T] Added to Blacklist: " + std::to_wstring(fp));
                    }
                }
            }
        }

        return true;
    }
}

bool DeviceApprovalFlow::ApproveDevicesFromPaths(HWND hWnd, const std::vector<std::wstring>& paths)
{
    std::vector<PendingGroup> groups;
    groups.reserve(paths.size());

    std::unordered_map<uint64_t, size_t> groupIndexByKey;
    groupIndexByKey.reserve(paths.size());

    for (const auto& path : paths) {
        if (path.empty()) {
            continue;
        }

        UsbItem item = HidFingerprint::BuildUsbItemFromPath(path);

        // whitelist 미포함 fingerprint 후보 규칙
        if (Policy::IsWhitelisted(item.fingerprint)) {
            continue;
        }

        AddOrUpdateGroup(groups, groupIndexByKey, std::move(item));
    }

    if (groups.empty()) {
        Logger::Add(L"[Approval] No unknown HID devices.");
        return false;
    }

    SortGroups(groups);
    FinalizeGroupDisplay(groups);

    for (const auto& group : groups) {
        Logger::Add(L"[Approval] Candidate: " + group.displayItem.name);
    }

    return RunApprovalDialog(hWnd, groups);
}

bool DeviceApprovalFlow::ApproveDeviceFromPath(HWND hWnd, const std::wstring& path)
{
    // hotplug 입력 기반 현재 연결 경로 확장
    auto paths = HidInitialScan::EnumeratePresentHidDevicePaths();

    // 열거 지연 대비 도착 경로 강제 포함
    if (!path.empty()) {
        bool exists = false;
        for (const auto& p : paths) {
            if (_wcsicmp(p.c_str(), path.c_str()) == 0) {
                exists = true;
                break;
            }
        }

        if (!exists) {
            paths.push_back(path);
        }
    }

    return ApproveDevicesFromPaths(hWnd, paths);
}
