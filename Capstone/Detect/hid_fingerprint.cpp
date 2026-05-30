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

    std::wstring BuildPathFallbackIdentity(const std::wstring& devicePath)
    {
        // ContainerId 도, 부모 chain 의 USB\VID_... 도 못 잡은 경우의
        // 마지막 fallback. 의도: "같은 디바이스 = 같은 fingerprint" 보장.
        //
        // 원시 devicePath 는 disable/enable 사이클마다 변하는 instance
        // suffix (port hash, sequence number 등) 를 끝에 달고 있다.
        // 이걸 그대로 hash 하면 같은 디바이스가 매번 다른 fingerprint
        // 를 받는다. 그래서 VID/PID 패턴 (vid_xxxx&pid_xxxx) 까지만
        // 남기고 그 뒤를 통째로 잘라낸다.
        //
        //   "usb\vid_046d&pid_c34b\6&3365fbaf&0&7"  → "usb\vid_046d&pid_c34b"
        //   "hid#vid_046d&pid_c34b&mi_00#..."       → "hid#vid_046d&pid_c34b"
        //   "bthenum\{guid}_vid&xxxx_pid&yyyy\..."  → 매칭 안 됨, 원본 유지
        //                                              (BT 는 path 자체가 안정적)
        std::wstring key = ToLowerCopy(devicePath);

        // vid_XXXX&pid_XXXX 패턴 위치 탐색 후 그 뒤 자르기.
        const size_t vidPos = key.find(L"vid_");
        if (vidPos == std::wstring::npos) {
            return key;  // BT 등 — 원본 사용
        }

        // vid_XXXX 까지 검증
        const size_t vidHexEnd = vidPos + 4;  // "vid_" 길이
        if (vidHexEnd + 4 > key.size()) {
            return key;
        }
        for (size_t i = 0; i < 4; i++) {
            if (!iswxdigit(key[vidHexEnd + i])) {
                return key;
            }
        }

        // &pid_XXXX 가 바로 뒤에 있어야
        const size_t pidTokPos = vidHexEnd + 4;
        const std::wstring pidTok = L"&pid_";
        if (pidTokPos + pidTok.size() + 4 > key.size()
            || key.compare(pidTokPos, pidTok.size(), pidTok) != 0) {
            return key;
        }
        const size_t pidHexStart = pidTokPos + pidTok.size();
        for (size_t i = 0; i < 4; i++) {
            if (!iswxdigit(key[pidHexStart + i])) {
                return key;
            }
        }

        // 여기까지 왔으면 vid_XXXX&pid_XXXX 패턴 확정. 그 끝까지만 남김.
        key.resize(pidHexStart + 4);
        return key;
    }

    uint64_t ComputeFnv1a64(const std::wstring& value)
    {
        uint64_t hash = 1469598103934665603ULL;
        for (wchar_t ch : value) {
            hash ^= static_cast<uint64_t>(ch);
            hash *= 1099511628211ULL;
        }
        return hash;
    }

    uint64_t BuildFingerprint(bool hasVidPid, uint16_t vid, uint16_t pid, const std::wstring& identityKey)
    {
        // ── 정책: VID/PID 만으로 fingerprint 를 결정 ──────────────────
        // 이전엔 (VID<<48) ^ (PID<<32) ^ (identityHash & 0xFFFFFFFF) 였는데
        // identityHash 는 결국 ContainerId 또는 InstanceId 기반인데, 두 값
        // 모두 USB 2.0 + iSerial 미보고 디바이스에 한해 Windows 가 enumeration
        // 마다 새로 발급한다. 같은 디바이스가 매 사이클마다 다른 fingerprint
        // 를 받아 whitelist/blacklist 가 망가지는 사례가 관측됐다.
        //
        // 정밀도를 VID/PID 수준으로 낮추면 같은 모델 디바이스 2개를 구분
        // 못 하는 단점이 있지만, BadUSB 가 어차피 VID/PID 위조가 자명하므로
        // 더 큰 정밀도는 의미 없다. 입력 패턴/명령 분석 (hook 단) 으로 보강.
        //
        // BT 등 VID/PID 가 없는 디바이스만 path 기반 hash 를 유지한다.
        if (!hasVidPid) {
            return ComputeFnv1a64(identityKey);
        }
        return (static_cast<uint64_t>(vid) << 48)
             | (static_cast<uint64_t>(pid) << 32);
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

    std::wstring GuidToString(const GUID& guid)
    {
        wchar_t buffer[64]{};
        swprintf_s(
            buffer,
            _countof(buffer),
            L"{%08lX-%04hX-%04hX-%02X%02X-%02X%02X%02X%02X%02X%02X}",
            static_cast<unsigned long>(guid.Data1),
            static_cast<unsigned short>(guid.Data2),
            static_cast<unsigned short>(guid.Data3),
            static_cast<unsigned int>(guid.Data4[0]),
            static_cast<unsigned int>(guid.Data4[1]),
            static_cast<unsigned int>(guid.Data4[2]),
            static_cast<unsigned int>(guid.Data4[3]),
            static_cast<unsigned int>(guid.Data4[4]),
            static_cast<unsigned int>(guid.Data4[5]),
            static_cast<unsigned int>(guid.Data4[6]),
            static_cast<unsigned int>(guid.Data4[7]));
        return buffer;
    }

    bool AssignNonEmptyWideString(const wchar_t* text, std::wstring& value)
    {
        if (!text || text[0] == L'\0') {
            return false;
        }

        value = text;
        return true;
    }

    template <typename Reader>
    bool ReadSetupDiWideString(Reader&& reader, std::wstring& value)
    {
        WCHAR localBuffer[256]{};
        DWORD requiredBytes = 0;

        if (reader(reinterpret_cast<PBYTE>(localBuffer), static_cast<DWORD>(sizeof(localBuffer)), &requiredBytes)) {
            return AssignNonEmptyWideString(localBuffer, value);
        }

        if (GetLastError() != ERROR_INSUFFICIENT_BUFFER || requiredBytes < sizeof(wchar_t)) {
            return false;
        }

        std::vector<BYTE> dynamicBuffer(requiredBytes, 0);
        if (!reader(dynamicBuffer.data(), static_cast<DWORD>(dynamicBuffer.size()), nullptr)) {
            return false;
        }

        return AssignNonEmptyWideString(reinterpret_cast<const wchar_t*>(dynamicBuffer.data()), value);
    }

    template <typename Reader>
    bool ReadCmWideString(Reader&& reader, std::wstring& value)
    {
        ULONG requiredBytes = 0;
        CONFIGRET cr = reader(nullptr, &requiredBytes);
        if (cr != CR_SUCCESS && cr != CR_BUFFER_SMALL) {
            return false;
        }

        if (requiredBytes < sizeof(wchar_t)) {
            return false;
        }

        std::vector<BYTE> dynamicBuffer(requiredBytes, 0);
        cr = reader(dynamicBuffer.data(), &requiredBytes);
        if (cr != CR_SUCCESS) {
            return false;
        }

        return AssignNonEmptyWideString(reinterpret_cast<const wchar_t*>(dynamicBuffer.data()), value);
    }

    bool ReadDeviceGuidProperty(
        HDEVINFO deviceInfoSet,
        SP_DEVINFO_DATA& deviceInfo,
        const DEVPROPKEY& key,
        GUID& value)
    {
        DEVPROPTYPE propType = 0;
        DWORD requiredBytes = 0;
        GUID localValue{};

        const BOOL ok = SetupDiGetDevicePropertyW(
            deviceInfoSet,
            &deviceInfo,
            &key,
            &propType,
            reinterpret_cast<PBYTE>(&localValue),
            static_cast<DWORD>(sizeof(localValue)),
            &requiredBytes,
            0);
        if (!ok) {
            return false;
        }

        if (propType != DEVPROP_TYPE_GUID || requiredBytes != sizeof(localValue)) {
            return false;
        }

        value = localValue;
        return true;
    }

    bool ReadDevNodeGuidProperty(DEVINST devInst, const DEVPROPKEY& key, GUID& value)
    {
        DEVPROPTYPE propType = 0;
        ULONG requiredBytes = 0;

        CONFIGRET cr = CM_Get_DevNode_PropertyW(devInst, &key, &propType, nullptr, &requiredBytes, 0);
        if (cr != CR_SUCCESS && cr != CR_BUFFER_SMALL) {
            return false;
        }

        if (requiredBytes != sizeof(GUID)) {
            return false;
        }

        GUID localValue{};
        cr = CM_Get_DevNode_PropertyW(
            devInst,
            &key,
            &propType,
            reinterpret_cast<PBYTE>(&localValue),
            &requiredBytes,
            0);
        if (cr != CR_SUCCESS) {
            return false;
        }

        if (propType != DEVPROP_TYPE_GUID) {
            return false;
        }

        value = localValue;
        return true;
    }

    bool ReadDevNodeInstanceId(DEVINST devInst, std::wstring& value)
    {
        ULONG charCount = 0;
        if (CM_Get_Device_ID_Size(&charCount, devInst, 0) != CR_SUCCESS) {
            return false;
        }

        if (charCount == 0) {
            return false;
        }

        std::vector<wchar_t> buffer(charCount + 1, L'\0');
        if (CM_Get_Device_IDW(devInst, buffer.data(), static_cast<ULONG>(buffer.size()), 0) != CR_SUCCESS) {
            return false;
        }

        value.assign(buffer.data());
        return !value.empty();
    }

    std::wstring ResolveDeviceIdentityKey(HDEVINFO deviceInfoSet, SP_DEVINFO_DATA& devInfo)
    {
        GUID containerId{};

        // ── 진짜 PnP DEVINST 변환 ────────────────────────────────────
        // ResolveDeviceFromInterfacePath 가 만든 HDEVINFO 는 빈 set 에
        // SetupDiOpenDeviceInterfaceW 로 interface 만 register 한 임시
        // 컨텍스트라, devInfo.DevInst 가 진짜 PnP DEVINST 와 다를 수
        // 있다. 그러면 SetupDiGetDevicePropertyW(ContainerId) 와
        // CM_Get_DevNode_PropertyW 둘 다 실패하고 InstanceId fallback
        // 으로 떨어지는데, InstanceId 는 disable/enable 사이클마다
        // suffix 가 변해서 같은 디바이스가 다른 fingerprint 를 받는
        // 버그를 유발한다.
        //
        // SetupDiGetDeviceInstanceIdW → CM_Locate_DevNodeW 로 진짜
        // PnP DEVINST 를 얻은 뒤 ContainerId 를 조회해야 안정적이다.
        // ────────────────────────────────────────────────────────────
        DEVINST resolvedStart = devInfo.DevInst;
        {
            wchar_t instIdBuf[MAX_DEVICE_ID_LEN]{};
            if (SetupDiGetDeviceInstanceIdW(deviceInfoSet, &devInfo,
                                            instIdBuf, MAX_DEVICE_ID_LEN, nullptr)) {
                DEVINST located = 0;
                if (CM_Locate_DevNodeW(&located, instIdBuf, CM_LOCATE_DEVNODE_NORMAL) == CR_SUCCESS) {
                    resolvedStart = located;
                }
            }
        }

        // 진짜 DEVINST 로 ContainerId 직접 시도.
        if (ReadDevNodeGuidProperty(resolvedStart, DEVPKEY_Device_ContainerId, containerId)) {
            return L"cid:" + ToLowerCopy(GuidToString(containerId));
        }

        // SetupDi 경로로도 한 번 더 (혹시 위가 안 잡혔을 때 안전망).
        if (ReadDeviceGuidProperty(deviceInfoSet, devInfo, DEVPKEY_Device_ContainerId, containerId)) {
            return L"cid:" + ToLowerCopy(GuidToString(containerId));
        }

        // ── 부모 chain 8단계까지 ContainerId 탐색 ────────────────────
        DEVINST current = resolvedStart;
        std::wstring firstInstanceId;
        std::wstring hidInstanceId;

        for (int depth = 0; depth < 8; depth++) {
            if (ReadDevNodeGuidProperty(current, DEVPKEY_Device_ContainerId, containerId)) {
                return L"cid:" + ToLowerCopy(GuidToString(containerId));
            }

            std::wstring instanceId;
            if (ReadDevNodeInstanceId(current, instanceId)) {
                const std::wstring lowered = ToLowerCopy(instanceId);
                if (firstInstanceId.empty()) {
                    firstInstanceId = lowered;
                }

                if (lowered.find(L"usb\\vid_") != std::wstring::npos) {
                    return L"inst:" + lowered;
                }

                if (hidInstanceId.empty() && lowered.find(L"hid\\vid_") != std::wstring::npos) {
                    hidInstanceId = lowered;
                }
            }

            DEVINST parent = 0;
            if (CM_Get_Parent(&parent, current, 0) != CR_SUCCESS) {
                break;
            }
            current = parent;
        }

        if (!hidInstanceId.empty()) {
            return L"inst:" + hidInstanceId;
        }

        if (!firstInstanceId.empty()) {
            return L"inst:" + firstInstanceId;
        }

        return L"";
    }

    // SetupAPI 레지스트리 문자열 속성 조회
    bool ReadDeviceRegistryProperty(
        HDEVINFO deviceInfoSet,
        SP_DEVINFO_DATA& deviceInfo,
        DWORD property,
        std::wstring& value)
    {
        return ReadSetupDiWideString(
            [&](PBYTE buffer, DWORD bufferBytes, DWORD* requiredBytes) {
                return SetupDiGetDeviceRegistryPropertyW(
                    deviceInfoSet,
                    &deviceInfo,
                    property,
                    nullptr,
                    buffer,
                    bufferBytes,
                    requiredBytes);
            },
            value);
    }

    // SetupAPI DEVPROPKEY 문자열 속성 조회
    bool ReadDeviceProperty(
        HDEVINFO deviceInfoSet,
        SP_DEVINFO_DATA& deviceInfo,
        const DEVPROPKEY& key,
        std::wstring& value)
    {
        DEVPROPTYPE propType = 0;

        return ReadSetupDiWideString(
            [&](PBYTE buffer, DWORD bufferBytes, DWORD* requiredBytes) {
                return SetupDiGetDevicePropertyW(
                    deviceInfoSet,
                    &deviceInfo,
                    &key,
                    &propType,
                    buffer,
                    bufferBytes,
                    requiredBytes,
                    0);
            },
            value);
    }

    // devnode DEVPROPKEY 문자열 속성 조회
    bool ReadDevNodeProperty(DEVINST devInst, const DEVPROPKEY& key, std::wstring& value)
    {
        DEVPROPTYPE propType = 0;
        return ReadCmWideString(
            [&](PBYTE buffer, PULONG requiredBytes) {
                return CM_Get_DevNode_PropertyW(devInst, &key, &propType, buffer, requiredBytes, 0);
            },
            value);
    }

    // HID 문자열 디스크립터 조회
    bool ReadHidString(
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

    template <typename TKey, typename Reader>
    void AddCandidateByKey(const TKey& key, Reader&& reader, std::vector<std::wstring>& candidates)
    {
        std::wstring value;
        if (reader(key, value)) {
            AddCandidateName(candidates, value);
        }
    }

    // HID 문자열 기반 이름 후보 추가
    void CollectHidStringCandidates(const std::wstring& devicePath, std::vector<std::wstring>& candidates)
    {
        std::wstring product;
        std::wstring manufacturer;

        ReadHidString(devicePath, HidD_GetProductString, product);
        ReadHidString(devicePath, HidD_GetManufacturerString, manufacturer);

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
    void CollectCurrentNodeCandidates(
        HDEVINFO deviceInfoSet,
        SP_DEVINFO_DATA& devInfo,
        std::vector<std::wstring>& candidates)
    {
        static const DEVPROPKEY kDevicePropertyKeys[] = {
            DEVPKEY_Device_BusReportedDeviceDesc,
            DEVPKEY_NAME,
            DEVPKEY_Device_FriendlyName,
            DEVPKEY_Device_DeviceDesc
        };

        static const DWORD kRegistryProperties[] = {
            SPDRP_FRIENDLYNAME,
            SPDRP_DEVICEDESC,
            SPDRP_MFG
        };

        auto readDeviceProperty = [&](const DEVPROPKEY& key, std::wstring& value) {
            return ReadDeviceProperty(deviceInfoSet, devInfo, key, value);
        };
        for (const auto& key : kDevicePropertyKeys) {
            AddCandidateByKey(key, readDeviceProperty, candidates);
        }

        auto readRegistryProperty = [&](DWORD property, std::wstring& value) {
            return ReadDeviceRegistryProperty(deviceInfoSet, devInfo, property, value);
        };
        for (DWORD property : kRegistryProperties) {
            AddCandidateByKey(property, readRegistryProperty, candidates);
        }
    }

    // 상위 devnode 기반 이름 후보 추가
    void CollectAncestorNodeCandidates(DEVINST startDevInst, std::vector<std::wstring>& candidates)
    {
        static const DEVPROPKEY kAncestorPropertyKeys[] = {
            DEVPKEY_Device_BusReportedDeviceDesc,
            DEVPKEY_Device_FriendlyName,
            DEVPKEY_NAME,
            DEVPKEY_Device_DeviceDesc
        };

        DEVINST current = startDevInst;
        for (int depth = 0; depth < 6; depth++) {
            auto readDevNodeProperty = [&](const DEVPROPKEY& key, std::wstring& value) {
                return ReadDevNodeProperty(current, key, value);
            };
            for (const auto& key : kAncestorPropertyKeys) {
                AddCandidateByKey(key, readDevNodeProperty, candidates);
            }

            DEVINST parent = 0;
            if (CM_Get_Parent(&parent, current, 0) != CR_SUCCESS) {
                break;
            }
            current = parent;
        }
    }

    // 일반명 제외 우선 선택 처리
    std::wstring SelectBestCandidateName(const std::vector<std::wstring>& candidates)
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

    // HID 인터페이스 경로 기반 표시명/식별키 해석
    std::wstring ResolveDeviceMetadataFromPath(const std::wstring& devicePath, std::wstring* outIdentityKey)
    {
        if (outIdentityKey != nullptr) {
            outIdentityKey->clear();
        }

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
            CollectHidStringCandidates(devicePath, candidates);
            CollectCurrentNodeCandidates(deviceInfoSet, devInfo, candidates);
            CollectAncestorNodeCandidates(devInfo.DevInst, candidates);

            if (outIdentityKey != nullptr) {
                *outIdentityKey = ResolveDeviceIdentityKey(deviceInfoSet, devInfo);
            }

            resolvedName = SelectBestCandidateName(candidates);
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
    bool ExtractVidPid(const std::wstring& devicePath, uint16_t& vid, uint16_t& pid)
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

    uint64_t ComputeStableHash64(const std::wstring& value)
    {
        return ComputeFnv1a64(value);
    }

    UsbItem BuildUsbItemFromPath(const std::wstring& devicePath)
    {
        UsbItem item;
        item.devicePath = devicePath;

        uint16_t vid = 0;
        uint16_t pid = 0;
        const bool hasVidPid = ExtractVidPid(devicePath, vid, pid);
        item.vid = vid;
        item.pid = pid;

        std::wstring identityKey;
        item.name = ResolveDeviceMetadataFromPath(devicePath, &identityKey);

        // 기기 단위 키 계산: VID/PID + 식별키 해시 하위 32비트
        if (identityKey.empty()) {
            identityKey = BuildPathFallbackIdentity(devicePath);
        }
        item.fingerprint = BuildFingerprint(hasVidPid, vid, pid, identityKey);

        if (item.name.empty()) {
            item.name = BuildFallbackName(hasVidPid, vid, pid);
        }

        item.allow = false;
        return item;
    }
}
