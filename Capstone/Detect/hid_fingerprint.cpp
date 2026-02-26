#include "hid_fingerprint.h"

#include <windows.h>
#include <cfgmgr32.h>
#include <initguid.h>
#include <hidsdi.h>
#include <devpkey.h>
#include <hidclass.h>
#include <setupapi.h>

#include <cwchar>
#include <cwctype>
#include <iomanip>
#include <sstream>
#include <string>
#include <vector>

#pragma comment(lib, "Cfgmgr32.lib")
#pragma comment(lib, "Hid.lib")
#pragma comment(lib, "Setupapi.lib")

namespace
{
    std::wstring TrimCopy(const std::wstring& value)
    {
        const auto first = value.find_first_not_of(L" \t\r\n");
        if (first == std::wstring::npos) {
            return L"";
        }

        const auto last = value.find_last_not_of(L" \t\r\n");
        return value.substr(first, last - first + 1);
    }

    std::wstring ToLowerCopy(const std::wstring& value)
    {
        std::wstring out = value;
        for (auto& ch : out) {
            ch = static_cast<wchar_t>(towlower(ch));
        }
        return out;
    }

    bool IsSamePathIgnoreCase(const std::wstring& left, const std::wstring& right)
    {
        return _wcsicmp(left.c_str(), right.c_str()) == 0;
    }

    // 일반 HID 인터페이스 라벨 판별
    bool LooksLikeGenericHidLabel(const std::wstring& value)
    {
        const std::wstring s = ToLowerCopy(value);

        const wchar_t* genericTokens[] = {
            L"hid compliant",
            L"hid-compliant",
            L"hid keyboard",
            L"hid mouse",
            L"hid device",
            L"vendor-defined",
            L"consumer control",
            L"system controller",
            L"hid \uaddc\uaca9",
            L"hid \uc900\uc218",
            L"\ud0a4\ubcf4\ub4dc \uc7a5\uce58",
            L"\ub9c8\uc6b0\uc2a4",
            L"\uc0ac\uc6a9\uc790 \ucee8\ud2b8\ub864",
            L"\uc2dc\uc2a4\ud15c \ucee8\ud2b8\ub864\ub7ec",
            L"\uacf5\uae09\uc5c5\uccb4 \uc815\uc758"
        };

        for (const auto* token : genericTokens) {
            if (s.find(token) != std::wstring::npos) {
                return true;
            }
        }

        return false;
    }

    void AddCandidateName(std::vector<std::wstring>& candidates, const std::wstring& rawName)
    {
        const std::wstring name = TrimCopy(rawName);
        if (name.empty()) {
            return;
        }

        for (const auto& existing : candidates) {
            if (_wcsicmp(existing.c_str(), name.c_str()) == 0) {
                return;
            }
        }

        candidates.push_back(name);
    }

    // SetupAPI 레지스트리 문자열 속성 조회
    bool TryReadDeviceRegistryProperty(
        HDEVINFO deviceInfoSet,
        SP_DEVINFO_DATA& deviceInfo,
        DWORD property,
        std::wstring& value)
    {
        WCHAR localBuffer[256]{};
        DWORD requiredBytes = 0;

        if (SetupDiGetDeviceRegistryPropertyW(
            deviceInfoSet,
            &deviceInfo,
            property,
            nullptr,
            reinterpret_cast<PBYTE>(localBuffer),
            static_cast<DWORD>(sizeof(localBuffer)),
            &requiredBytes))
        {
            value = localBuffer;
            return !value.empty();
        }

        if (GetLastError() != ERROR_INSUFFICIENT_BUFFER || requiredBytes < sizeof(wchar_t)) {
            return false;
        }

        std::vector<BYTE> buffer(requiredBytes, 0);
        if (!SetupDiGetDeviceRegistryPropertyW(
            deviceInfoSet,
            &deviceInfo,
            property,
            nullptr,
            buffer.data(),
            static_cast<DWORD>(buffer.size()),
            nullptr))
        {
            return false;
        }

        const auto* text = reinterpret_cast<const wchar_t*>(buffer.data());
        if (!text || text[0] == L'\0') {
            return false;
        }

        value = text;
        return true;
    }

    // SetupAPI DEVPROPKEY 문자열 속성 조회
    bool TryReadDeviceProperty(
        HDEVINFO deviceInfoSet,
        SP_DEVINFO_DATA& deviceInfo,
        const DEVPROPKEY& key,
        std::wstring& value)
    {
        WCHAR localBuffer[256]{};
        DEVPROPTYPE propType = 0;
        DWORD requiredBytes = 0;

        if (SetupDiGetDevicePropertyW(
            deviceInfoSet,
            &deviceInfo,
            &key,
            &propType,
            reinterpret_cast<PBYTE>(localBuffer),
            static_cast<DWORD>(sizeof(localBuffer)),
            &requiredBytes,
            0))
        {
            value = localBuffer;
            return !value.empty();
        }

        if (GetLastError() != ERROR_INSUFFICIENT_BUFFER || requiredBytes < sizeof(wchar_t)) {
            return false;
        }

        std::vector<BYTE> buffer(requiredBytes, 0);
        if (!SetupDiGetDevicePropertyW(
            deviceInfoSet,
            &deviceInfo,
            &key,
            &propType,
            buffer.data(),
            static_cast<DWORD>(buffer.size()),
            nullptr,
            0))
        {
            return false;
        }

        const auto* text = reinterpret_cast<const wchar_t*>(buffer.data());
        if (!text || text[0] == L'\0') {
            return false;
        }

        value = text;
        return true;
    }

    // devnode DEVPROPKEY 문자열 속성 조회
    bool TryReadDevNodeProperty(DEVINST devInst, const DEVPROPKEY& key, std::wstring& value)
    {
        DEVPROPTYPE propType = 0;
        ULONG requiredBytes = 0;

        CONFIGRET cr = CM_Get_DevNode_PropertyW(devInst, &key, &propType, nullptr, &requiredBytes, 0);
        if (cr != CR_SUCCESS && cr != CR_BUFFER_SMALL) {
            return false;
        }

        if (requiredBytes < sizeof(wchar_t)) {
            return false;
        }

        std::vector<BYTE> buffer(requiredBytes, 0);
        cr = CM_Get_DevNode_PropertyW(devInst, &key, &propType, buffer.data(), &requiredBytes, 0);
        if (cr != CR_SUCCESS) {
            return false;
        }

        const auto* text = reinterpret_cast<const wchar_t*>(buffer.data());
        if (!text || text[0] == L'\0') {
            return false;
        }

        value = text;
        return true;
    }

    // HID 문자열 디스크립터 조회
    bool TryReadHidString(
        const std::wstring& devicePath,
        BOOLEAN(__stdcall* readFunc)(HANDLE, PVOID, ULONG),
        std::wstring& value)
    {
        HANDLE hDevice = CreateFileW(
            devicePath.c_str(),
            0,
            FILE_SHARE_READ | FILE_SHARE_WRITE,
            nullptr,
            OPEN_EXISTING,
            FILE_ATTRIBUTE_NORMAL,
            nullptr);

        if (hDevice == INVALID_HANDLE_VALUE) {
            return false;
        }

        WCHAR buffer[512]{};
        const bool ok = (readFunc(hDevice, buffer, static_cast<ULONG>(sizeof(buffer))) == TRUE);
        CloseHandle(hDevice);

        if (!ok || buffer[0] == L'\0') {
            return false;
        }

        value = buffer;
        return true;
    }

    // HID 문자열 기반 이름 후보 추가
    void AddHidProductCandidates(const std::wstring& devicePath, std::vector<std::wstring>& candidates)
    {
        std::wstring product;
        std::wstring manufacturer;

        TryReadHidString(devicePath, HidD_GetProductString, product);
        TryReadHidString(devicePath, HidD_GetManufacturerString, manufacturer);

        AddCandidateName(candidates, product);

        if (!manufacturer.empty() && !product.empty()) {
            const std::wstring mfgLower = ToLowerCopy(manufacturer);
            const std::wstring productLower = ToLowerCopy(product);
            if (productLower.find(mfgLower) != 0) {
                AddCandidateName(candidates, manufacturer + L" " + product);
            }
        }

        AddCandidateName(candidates, manufacturer);
    }

    // 현재 HID 인터페이스 노드 기반 이름 후보 추가
    void AddCurrentNodeCandidates(
        HDEVINFO deviceInfoSet,
        SP_DEVINFO_DATA& devInfo,
        std::vector<std::wstring>& candidates)
    {
        std::wstring value;

        if (TryReadDeviceProperty(deviceInfoSet, devInfo, DEVPKEY_Device_BusReportedDeviceDesc, value)) {
            AddCandidateName(candidates, value);
        }
        if (TryReadDeviceProperty(deviceInfoSet, devInfo, DEVPKEY_NAME, value)) {
            AddCandidateName(candidates, value);
        }
        if (TryReadDeviceProperty(deviceInfoSet, devInfo, DEVPKEY_Device_FriendlyName, value)) {
            AddCandidateName(candidates, value);
        }
        if (TryReadDeviceProperty(deviceInfoSet, devInfo, DEVPKEY_Device_DeviceDesc, value)) {
            AddCandidateName(candidates, value);
        }

        if (TryReadDeviceRegistryProperty(deviceInfoSet, devInfo, SPDRP_FRIENDLYNAME, value)) {
            AddCandidateName(candidates, value);
        }
        if (TryReadDeviceRegistryProperty(deviceInfoSet, devInfo, SPDRP_DEVICEDESC, value)) {
            AddCandidateName(candidates, value);
        }
        if (TryReadDeviceRegistryProperty(deviceInfoSet, devInfo, SPDRP_MFG, value)) {
            AddCandidateName(candidates, value);
        }
    }

    // 상위 devnode 기반 이름 후보 추가
    void AddAncestorNodeCandidates(DEVINST startDevInst, std::vector<std::wstring>& candidates)
    {
        DEVINST current = startDevInst;
        for (int depth = 0; depth < 6; depth++) {
            std::wstring value;

            if (TryReadDevNodeProperty(current, DEVPKEY_Device_BusReportedDeviceDesc, value)) {
                AddCandidateName(candidates, value);
            }
            if (TryReadDevNodeProperty(current, DEVPKEY_Device_FriendlyName, value)) {
                AddCandidateName(candidates, value);
            }
            if (TryReadDevNodeProperty(current, DEVPKEY_NAME, value)) {
                AddCandidateName(candidates, value);
            }
            if (TryReadDevNodeProperty(current, DEVPKEY_Device_DeviceDesc, value)) {
                AddCandidateName(candidates, value);
            }

            DEVINST parent = 0;
            if (CM_Get_Parent(&parent, current, 0) != CR_SUCCESS) {
                break;
            }
            current = parent;
        }
    }

    // 일반명 제외 우선 선택 처리
    std::wstring PickBestCandidateName(const std::vector<std::wstring>& candidates)
    {
        for (const auto& name : candidates) {
            if (!LooksLikeGenericHidLabel(name)) {
                return name;
            }
        }

        if (!candidates.empty()) {
            return candidates.front();
        }

        return L"";
    }

    // HID 인터페이스 경로 기반 표시명 해석
    std::wstring ResolveDeviceDisplayNameFromPath(const std::wstring& devicePath)
    {
        HDEVINFO deviceInfoSet = SetupDiGetClassDevsW(
            &GUID_DEVINTERFACE_HID,
            nullptr,
            nullptr,
            DIGCF_PRESENT | DIGCF_DEVICEINTERFACE);

        if (deviceInfoSet == INVALID_HANDLE_VALUE) {
            return L"";
        }

        SP_DEVICE_INTERFACE_DATA ifData{};
        ifData.cbSize = sizeof(ifData);

        std::wstring resolvedName;

        for (DWORD i = 0; SetupDiEnumDeviceInterfaces(deviceInfoSet, nullptr, &GUID_DEVINTERFACE_HID, i, &ifData); i++)
        {
            DWORD required = 0;
            SetupDiGetDeviceInterfaceDetailW(deviceInfoSet, &ifData, nullptr, 0, &required, nullptr);
            if (required == 0) {
                continue;
            }

            std::vector<BYTE> buffer(required);
            auto* detail = reinterpret_cast<SP_DEVICE_INTERFACE_DETAIL_DATA_W*>(buffer.data());
            detail->cbSize = sizeof(SP_DEVICE_INTERFACE_DETAIL_DATA_W);

            SP_DEVINFO_DATA devInfo{};
            devInfo.cbSize = sizeof(devInfo);

            if (!SetupDiGetDeviceInterfaceDetailW(
                deviceInfoSet,
                &ifData,
                detail,
                required,
                nullptr,
                &devInfo))
            {
                continue;
            }

            if (!detail->DevicePath || !IsSamePathIgnoreCase(devicePath, detail->DevicePath)) {
                continue;
            }

            std::vector<std::wstring> candidates;
            AddHidProductCandidates(devicePath, candidates);
            AddCurrentNodeCandidates(deviceInfoSet, devInfo, candidates);
            AddAncestorNodeCandidates(devInfo.DevInst, candidates);

            resolvedName = PickBestCandidateName(candidates);
            break;
        }

        SetupDiDestroyDeviceInfoList(deviceInfoSet);
        return resolvedName;
    }

    // 이름 미확보 시 폴백 이름 생성
    std::wstring BuildFallbackName(bool hasVidPid, uint16_t vid, uint16_t pid)
    {
        if (!hasVidPid) {
            return L"Unknown HID Device";
        }

        std::wstringstream ss;
        ss << L"HID Device (VID_"
            << std::uppercase << std::setfill(L'0') << std::setw(4) << std::hex << vid
            << L"&PID_"
            << std::uppercase << std::setfill(L'0') << std::setw(4) << std::hex << pid
            << L")";
        return ss.str();
    }
}

namespace HidFingerprint
{
    bool TryExtractVidPid(const std::wstring& devicePath, uint16_t& vid, uint16_t& pid)
    {
        std::wstring normalized = devicePath;
        for (auto& ch : normalized) {
            ch = static_cast<wchar_t>(towupper(ch));
        }

        const size_t vidPos = normalized.find(L"VID_");
        const size_t pidPos = normalized.find(L"PID_");
        if (vidPos == std::wstring::npos || pidPos == std::wstring::npos) return false;
        if (vidPos + 8 > normalized.size() || pidPos + 8 > normalized.size()) return false;

        vid = static_cast<uint16_t>(wcstoul(normalized.substr(vidPos + 4, 4).c_str(), nullptr, 16));
        pid = static_cast<uint16_t>(wcstoul(normalized.substr(pidPos + 4, 4).c_str(), nullptr, 16));
        return true;
    }

    uint64_t ComputePathHash64(const std::wstring& value)
    {
        uint64_t hash = 1469598103934665603ULL;
        for (wchar_t ch : value) {
            hash ^= static_cast<uint64_t>(ch);
            hash *= 1099511628211ULL;
        }
        return hash;
    }

    UsbItem BuildUsbItemFromPath(const std::wstring& devicePath)
    {
        UsbItem item;
        item.devicePath = devicePath;

        uint16_t vid = 0;
        uint16_t pid = 0;
        const bool hasVidPid = TryExtractVidPid(devicePath, vid, pid);
        item.vid = vid;
        item.pid = pid;

        // 장치 키 계산: VID/PID + DevicePath 해시 하위 32비트
        if (hasVidPid) {
            const uint64_t pathHash = ComputePathHash64(devicePath);
            item.fingerprint = (static_cast<uint64_t>(vid) << 48) ^ (static_cast<uint64_t>(pid) << 32) ^ (pathHash & 0xFFFFFFFFULL);
        }
        else {
            item.fingerprint = ComputePathHash64(devicePath);
        }

        item.name = ResolveDeviceDisplayNameFromPath(devicePath);
        if (item.name.empty()) {
            item.name = BuildFallbackName(hasVidPid, vid, pid);
        }

        item.allow = false;
        return item;
    }
}
