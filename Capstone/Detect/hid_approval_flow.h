#pragma once
#include <windows.h>
#include <string>
#include <vector>

namespace DeviceApprovalFlow {
    // 다중 HID 경로 기반 승인 후보 생성 및 승인 창 표시
    bool ApproveDevicesFromPaths(HWND hWnd, const std::vector<std::wstring>& paths);

    // 단일 hotplug 경로 입력 처리 진입점
    bool ApproveDeviceFromPath(HWND hWnd, const std::wstring& path);
}
