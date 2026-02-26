#pragma once
#include <string>
#include <cstdint>

struct UsbItem {
    uint16_t vid = 0;
    uint16_t pid = 0;
    uint64_t fingerprint = 0;

    std::wstring devicePath; // 원본 path(로그/추적용)
    std::wstring name;       // optional (없으면 빈 문자열)
    bool allow = false;      // 체크됨=Whitelist, 미체크=Blacklist
};