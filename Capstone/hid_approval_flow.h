#pragma once
#include <windows.h>
#include <string>
#include <vector>

namespace DeviceApprovalFlow {
    // 여러 경로 한번에 (initial scan용)
    bool ApproveFromPaths(HWND hWnd, const std::vector<std::wstring>& paths);

    // 단일 경로 (hotplug용)
    bool ApproveFromPath(HWND hWnd, const std::wstring& path);
}