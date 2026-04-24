#pragma once
#include <vector>
#include <string>

namespace HidScan {
    // 현재 연결된 HID 인터페이스 경로 열거
    std::vector<std::wstring> EnumeratePresentHidDevicePaths();
}
