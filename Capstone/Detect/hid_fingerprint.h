#pragma once

#include <cstdint>
#include <string>

#include "usb_item.h"

namespace HidFingerprint {
    // DevicePath 기반 VID/PID 토큰 추출
    bool ExtractVidPid(const std::wstring& devicePath, uint16_t& vid, uint16_t& pid);

    // 문자열 기반 64비트 해시 계산
    uint64_t ComputeStableHash64(const std::wstring& value);
    // 하위 호환용 별칭
    inline uint64_t ComputePathHash64(const std::wstring& value) { return ComputeStableHash64(value); }

    // VID/PID, fingerprint, 표시명 포함 UsbItem 생성
    UsbItem BuildUsbItemFromPath(const std::wstring& devicePath);
}
