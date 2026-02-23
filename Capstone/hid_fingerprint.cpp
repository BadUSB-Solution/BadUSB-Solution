#include "hid_fingerprint.h"
#include <cstdlib>

namespace HidFingerprint
{
    static bool ExtractVidPid(const std::wstring& path, uint16_t& vid, uint16_t& pid)
    {
        // 대소문자 무시 검색을 위해 복사본을 대문자로
        std::wstring up = path;
        for (auto& ch : up) ch = (wchar_t)towupper(ch);

        size_t v = up.find(L"VID_");
        size_t p = up.find(L"PID_");
        if (v == std::wstring::npos || p == std::wstring::npos) return false;

        if (v + 8 > up.size() || p + 8 > up.size()) return false;

        vid = (uint16_t)wcstoul(up.substr(v + 4, 4).c_str(), nullptr, 16);
        pid = (uint16_t)wcstoul(up.substr(p + 4, 4).c_str(), nullptr, 16);
        return true;
    }

    uint64_t Hash64_FNV1a(const std::wstring& s)
    {
        uint64_t h = 1469598103934665603ULL;
        for (wchar_t c : s) { h ^= (uint64_t)c; h *= 1099511628211ULL; }
        return h;
    }

    UsbItem BuildItem(const std::wstring& devicePath)
    {
        UsbItem item;
        item.devicePath = devicePath;

        uint16_t vid = 0, pid = 0;
        bool ok = ExtractVidPid(devicePath, vid, pid);
        item.vid = vid;
        item.pid = pid;

        // 장치 단위 구분: VID/PID + (devicePath hash 일부)
        if (ok) {
            uint64_t h = Hash64_FNV1a(devicePath);
            item.fingerprint = ((uint64_t)vid << 48) ^ ((uint64_t)pid << 32) ^ (h & 0xFFFFFFFFULL);
        }
        else {
            item.fingerprint = Hash64_FNV1a(devicePath);
        }

        item.name = L"";
        item.allow = false;
        return item;
    }
}