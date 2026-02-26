#pragma once
#pragma once
#include <string>
#include <cstdint>
#include "usb_item.h"

namespace HidFingerprint {
    bool ExtractVidPid(const std::wstring& path, uint16_t& vid, uint16_t& pid);
    uint64_t Hash64_FNV1a(const std::wstring& s);
    UsbItem BuildItem(const std::wstring& devicePath);
}