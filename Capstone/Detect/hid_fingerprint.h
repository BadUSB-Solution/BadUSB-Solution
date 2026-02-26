#pragma once

#include <cstdint>
#include <string>

#include "usb_item.h"

namespace HidFingerprint {
    // DevicePath 기반 VID/PID 토큰 추출
    bool ExtractVidPid(const std::wstring& devicePath, uint16_t& vid, uint16_t& pid);

    // 경로 기반 64비트 해시 계산
    uint64_t ComputePathHash64(const std::wstring& value);

    // VID/PID, fingerprint, 표시명 포함 UsbItem 생성
    UsbItem BuildUsbItemFromPath(const std::wstring& devicePath);
}
