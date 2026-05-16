#include "device_finder.h"

#include <algorithm>
#include <iomanip>
#include <cwctype>
#include <sstream>

#pragma comment(lib, "Cfgmgr32.lib")
#pragma comment(lib, "Setupapi.lib")

namespace
{
    void SetDetail(std::wstring* detail, const std::wstring& value)
    {
        if (detail != nullptr) {
            *detail = value;
        }
    }

    bool ReadDeviceInstanceId(DEVINST devInst, std::wstring& instanceId)
    {
        // DEVINST에서 장치 관리자에 보이는 Instance ID 문자열을 읽는다.
        ULONG charCount = 0;
        if (CM_Get_Device_ID_Size(&charCount, devInst, 0) != CR_SUCCESS || charCount == 0) {
            return false;
        }

        std::vector<wchar_t> buffer(charCount + 1, L'\0');
        if (CM_Get_Device_IDW(devInst, buffer.data(), static_cast<ULONG>(buffer.size()), 0) != CR_SUCCESS) {
            return false;
        }

        instanceId.assign(buffer.data());
        return !instanceId.empty();
    }

    std::wstring ToLowerCopy(const std::wstring& value)
    {
        std::wstring lowered = value;
        for (auto& ch : lowered) {
            ch = static_cast<wchar_t>(towlower(ch));
        }
        return lowered;
    }

    int CandidatePriority(const std::wstring& instanceId)
    {
        const std::wstring lowered = ToLowerCopy(instanceId);
        // HID interface만 끄면 입력 장치가 살아남을 수 있어 USB composite 노드를 우선한다.
        if (lowered.rfind(L"usb\\vid_", 0) == 0) {
            return 0;
        }
        if (lowered.rfind(L"hid\\vid_", 0) == 0) {
            return 1;
        }
        if (DeviceFinder::IsVidPidNode(instanceId)) {
            return 2;
        }
        return 3;
    }

    std::wstring BuildVidPidNeedle(uint16_t vid, uint16_t pid)
    {
        std::wostringstream ss;
        ss << L"vid_"
            << std::uppercase << std::hex << std::setfill(L'0') << std::setw(4) << vid
            << L"&pid_"
            << std::uppercase << std::hex << std::setfill(L'0') << std::setw(4) << pid;
        return ToLowerCopy(ss.str());
    }
}

namespace DeviceFinder
{
    std::wstring FormatWin32Error(DWORD error)
    {
        wchar_t* buffer = nullptr;
        const DWORD length = FormatMessageW(
            FORMAT_MESSAGE_ALLOCATE_BUFFER |
            FORMAT_MESSAGE_FROM_SYSTEM |
            FORMAT_MESSAGE_IGNORE_INSERTS,
            nullptr,
            error,
            0,
            reinterpret_cast<LPWSTR>(&buffer),
            0,
            nullptr);

        if (length == 0 || buffer == nullptr) {
            return L"error=" + std::to_wstring(error);
        }

        std::wstring message(buffer, length);
        LocalFree(buffer);

        while (!message.empty() &&
            (message.back() == L'\r' || message.back() == L'\n' || message.back() == L' ')) {
            message.pop_back();
        }

        return message + L" (error=" + std::to_wstring(error) + L")";
    }

    void DestroyResolvedDevice(ResolvedDevice& resolved)
    {
        if (resolved.deviceInfoSet != INVALID_HANDLE_VALUE) {
            SetupDiDestroyDeviceInfoList(resolved.deviceInfoSet);
            resolved.deviceInfoSet = INVALID_HANDLE_VALUE;
        }
    }

    bool ResolveDeviceFromInterfacePath(
        const std::wstring& devicePath,
        ResolvedDevice& resolved,
        std::wstring* detail)
    {
        // WM_DEVICECHANGE로 받은 HID interface path를 SetupAPI devnode로 변환한다.
        // 이후 부모 USB 노드를 찾기 위한 시작점이다.
        resolved.devInfo.cbSize = sizeof(resolved.devInfo);
        resolved.deviceInfoSet = SetupDiCreateDeviceInfoList(nullptr, nullptr);
        if (resolved.deviceInfoSet == INVALID_HANDLE_VALUE) {
            SetDetail(detail, L"SetupDiCreateDeviceInfoList failed: " + FormatWin32Error(GetLastError()));
            return false;
        }

        SP_DEVICE_INTERFACE_DATA interfaceData{};
        interfaceData.cbSize = sizeof(interfaceData);
        if (!SetupDiOpenDeviceInterfaceW(
            resolved.deviceInfoSet,
            devicePath.c_str(),
            0,
            &interfaceData)) {
            SetDetail(detail, L"SetupDiOpenDeviceInterfaceW failed: " + FormatWin32Error(GetLastError()));
            return false;
        }

        DWORD requiredBytes = 0;
        SetupDiGetDeviceInterfaceDetailW(
            resolved.deviceInfoSet,
            &interfaceData,
            nullptr,
            0,
            &requiredBytes,
            nullptr);

        if (requiredBytes == 0) {
            SetDetail(detail, L"SetupDiGetDeviceInterfaceDetailW size query failed: " + FormatWin32Error(GetLastError()));
            return false;
        }

        std::vector<BYTE> buffer(requiredBytes);
        auto* detailData = reinterpret_cast<SP_DEVICE_INTERFACE_DETAIL_DATA_W*>(buffer.data());
        detailData->cbSize = sizeof(SP_DEVICE_INTERFACE_DETAIL_DATA_W);

        if (!SetupDiGetDeviceInterfaceDetailW(
            resolved.deviceInfoSet,
            &interfaceData,
            detailData,
            requiredBytes,
            nullptr,
            &resolved.devInfo)) {
            SetDetail(detail, L"SetupDiGetDeviceInterfaceDetailW failed: " + FormatWin32Error(GetLastError()));
            return false;
        }

        return true;
    }

    bool IsVidPidNode(const std::wstring& instanceId)
    {
        // 실제 USB/HID 장치 계열인지 판단하기 위한 최소 조건.
        const std::wstring lowered = ToLowerCopy(instanceId);
        return lowered.find(L"\\vid_") != std::wstring::npos &&
            lowered.find(L"&pid_") != std::wstring::npos;
    }

    std::vector<DevNodeCandidate> CollectBlockCandidates(DEVINST startDevInst)
    {
        std::vector<DevNodeCandidate> candidates;
        DEVINST current = startDevInst;

        // HID interface -> HID collection -> USB composite 순서로 부모를 따라 올라간다.
        // 실제 차단은 VID/PID가 들어간 devnode 중 우선순위가 높은 것부터 시도한다.
        for (int depth = 0; depth < 8; depth++) {
            std::wstring instanceId;
            if (ReadDeviceInstanceId(current, instanceId)) {
                candidates.push_back({ current, instanceId });
            }

            DEVINST parent = 0;
            if (CM_Get_Parent(&parent, current, 0) != CR_SUCCESS) {
                break;
            }
            current = parent;
        }

        std::stable_sort(candidates.begin(), candidates.end(),
            [](const DevNodeCandidate& left, const DevNodeCandidate& right) {
                return CandidatePriority(left.instanceId) < CandidatePriority(right.instanceId);
            });

        return candidates;
    }

    std::vector<std::wstring> FindVidPidInstanceIds(uint16_t vid, uint16_t pid)
    {
        std::vector<std::wstring> ids;
        if (vid == 0 && pid == 0) {
            return ids;
        }

        const std::wstring needle = BuildVidPidNeedle(vid, pid);
        HDEVINFO deviceInfoSet = SetupDiGetClassDevsW(
            nullptr,
            nullptr,
            nullptr,
            DIGCF_ALLCLASSES);
        if (deviceInfoSet == INVALID_HANDLE_VALUE) {
            return ids;
        }

        SP_DEVINFO_DATA devInfo{};
        devInfo.cbSize = sizeof(devInfo);
        for (DWORD i = 0; SetupDiEnumDeviceInfo(deviceInfoSet, i, &devInfo); ++i) {
            wchar_t instanceId[MAX_DEVICE_ID_LEN]{};
            if (!SetupDiGetDeviceInstanceIdW(
                deviceInfoSet,
                &devInfo,
                instanceId,
                _countof(instanceId),
                nullptr)) {
                continue;
            }

            const std::wstring id(instanceId);
            if (ToLowerCopy(id).find(needle) != std::wstring::npos) {
                ids.push_back(id);
            }
        }

        SetupDiDestroyDeviceInfoList(deviceInfoSet);
        std::stable_sort(ids.begin(), ids.end(),
            [](const std::wstring& left, const std::wstring& right) {
                return CandidatePriority(left) < CandidatePriority(right);
            });
        ids.erase(std::unique(ids.begin(), ids.end()), ids.end());
        return ids;
    }
}
