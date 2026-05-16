#include "device_blocker.h"

#include <windows.h>
#include <cfgmgr32.h>
#include <setupapi.h>

#include <string>
#include <vector>

#include "device_finder.h"
#include "logger.h"

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

    bool DisableDeviceNode(HDEVINFO deviceInfoSet, SP_DEVINFO_DATA& devInfo, std::wstring* detail)
    {
        // 장치 관리자에서 "사용 안 함"을 누르는 것과 같은 DIF_PROPERTYCHANGE 요청.
        SP_PROPCHANGE_PARAMS params{};
        params.ClassInstallHeader.cbSize = sizeof(SP_CLASSINSTALL_HEADER);
        params.ClassInstallHeader.InstallFunction = DIF_PROPERTYCHANGE;
        params.StateChange = DICS_DISABLE;
        params.Scope = DICS_FLAG_GLOBAL;
        params.HwProfile = 0;

        if (!SetupDiSetClassInstallParamsW(
            deviceInfoSet,
            &devInfo,
            &params.ClassInstallHeader,
            sizeof(params))) {
            SetDetail(detail, L"SetupDiSetClassInstallParamsW(DICS_DISABLE) failed: " +
                DeviceFinder::FormatWin32Error(GetLastError()));
            return false;
        }

        if (!SetupDiCallClassInstaller(DIF_PROPERTYCHANGE, deviceInfoSet, &devInfo)) {
            SetDetail(detail, L"SetupDiCallClassInstaller(DICS_DISABLE) failed: " +
                DeviceFinder::FormatWin32Error(GetLastError()));
            return false;
        }

        SetDetail(detail, L"device disabled");
        return true;
    }

    bool ChangeDeviceNodeState(
        HDEVINFO deviceInfoSet,
        SP_DEVINFO_DATA& devInfo,
        DWORD stateChange,
        const wchar_t* label,
        std::wstring* detail)
    {
        SP_PROPCHANGE_PARAMS params{};
        params.ClassInstallHeader.cbSize = sizeof(SP_CLASSINSTALL_HEADER);
        params.ClassInstallHeader.InstallFunction = DIF_PROPERTYCHANGE;
        params.StateChange = stateChange;
        params.Scope = DICS_FLAG_GLOBAL;
        params.HwProfile = 0;

        if (!SetupDiSetClassInstallParamsW(
            deviceInfoSet,
            &devInfo,
            &params.ClassInstallHeader,
            sizeof(params))) {
            SetDetail(detail, std::wstring(L"SetupDiSetClassInstallParamsW(") + label + L") failed: " +
                DeviceFinder::FormatWin32Error(GetLastError()));
            return false;
        }

        if (!SetupDiCallClassInstaller(DIF_PROPERTYCHANGE, deviceInfoSet, &devInfo)) {
            SetDetail(detail, std::wstring(L"SetupDiCallClassInstaller(") + label + L") failed: " +
                DeviceFinder::FormatWin32Error(GetLastError()));
            return false;
        }

        SetDetail(detail, std::wstring(L"device ") + label);
        return true;
    }

    bool DisableByInstanceId(const std::wstring& instanceId, std::wstring* detail)
    {
        // 부모 devnode는 처음 얻은 HDEVINFO에 없을 수 있어 instance id로 다시 연다.
        HDEVINFO deviceInfoSet = SetupDiCreateDeviceInfoList(nullptr, nullptr);
        if (deviceInfoSet == INVALID_HANDLE_VALUE) {
            SetDetail(detail, L"SetupDiCreateDeviceInfoList failed: " +
                DeviceFinder::FormatWin32Error(GetLastError()));
            return false;
        }

        SP_DEVINFO_DATA devInfo{};
        devInfo.cbSize = sizeof(devInfo);

        if (!SetupDiOpenDeviceInfoW(
            deviceInfoSet,
            instanceId.c_str(),
            nullptr,
            0,
            &devInfo)) {
            SetDetail(detail, L"SetupDiOpenDeviceInfoW failed for " + instanceId + L": " +
                DeviceFinder::FormatWin32Error(GetLastError()));
            SetupDiDestroyDeviceInfoList(deviceInfoSet);
            return false;
        }

        const bool disabled = DisableDeviceNode(deviceInfoSet, devInfo, detail);
        SetupDiDestroyDeviceInfoList(deviceInfoSet);
        return disabled;
    }

    bool EnableByInstanceId(const std::wstring& instanceId, std::wstring* detail)
    {
        HDEVINFO deviceInfoSet = SetupDiCreateDeviceInfoList(nullptr, nullptr);
        if (deviceInfoSet == INVALID_HANDLE_VALUE) {
            SetDetail(detail, L"SetupDiCreateDeviceInfoList failed: " +
                DeviceFinder::FormatWin32Error(GetLastError()));
            return false;
        }

        SP_DEVINFO_DATA devInfo{};
        devInfo.cbSize = sizeof(devInfo);

        if (!SetupDiOpenDeviceInfoW(
            deviceInfoSet,
            instanceId.c_str(),
            nullptr,
            0,
            &devInfo)) {
            SetDetail(detail, L"SetupDiOpenDeviceInfoW failed for " + instanceId + L": " +
                DeviceFinder::FormatWin32Error(GetLastError()));
            SetupDiDestroyDeviceInfoList(deviceInfoSet);
            return false;
        }

        std::wstring enableDetail;
        const bool enabled = ChangeDeviceNodeState(
            deviceInfoSet,
            devInfo,
            DICS_ENABLE,
            L"enabled",
            &enableDetail);
        SetupDiDestroyDeviceInfoList(deviceInfoSet);
        SetDetail(detail, enableDetail);
        return enabled;
    }

    bool RemoveByInstanceId(const std::wstring& instanceId, std::wstring* detail)
    {
        HDEVINFO deviceInfoSet = SetupDiCreateDeviceInfoList(nullptr, nullptr);
        if (deviceInfoSet == INVALID_HANDLE_VALUE) {
            SetDetail(detail, L"SetupDiCreateDeviceInfoList failed: " +
                DeviceFinder::FormatWin32Error(GetLastError()));
            return false;
        }

        SP_DEVINFO_DATA devInfo{};
        devInfo.cbSize = sizeof(devInfo);

        if (!SetupDiOpenDeviceInfoW(
            deviceInfoSet,
            instanceId.c_str(),
            nullptr,
            0,
            &devInfo)) {
            SetDetail(detail, L"SetupDiOpenDeviceInfoW failed for " + instanceId + L": " +
                DeviceFinder::FormatWin32Error(GetLastError()));
            SetupDiDestroyDeviceInfoList(deviceInfoSet);
            return false;
        }

        if (!SetupDiCallClassInstaller(DIF_REMOVE, deviceInfoSet, &devInfo)) {
            SetDetail(detail, L"SetupDiCallClassInstaller(DIF_REMOVE) failed for " + instanceId + L": " +
                DeviceFinder::FormatWin32Error(GetLastError()));
            SetupDiDestroyDeviceInfoList(deviceInfoSet);
            return false;
        }

        SetupDiDestroyDeviceInfoList(deviceInfoSet);
        SetDetail(detail, L"device removed for rescan");
        return true;
    }

    bool RescanRoot(std::wstring* detail)
    {
        DEVINST root = 0;
        CONFIGRET cr = CM_Locate_DevNodeW(&root, nullptr, CM_LOCATE_DEVNODE_NORMAL);
        if (cr != CR_SUCCESS) {
            SetDetail(detail, L"CM_Locate_DevNodeW(root) failed: cr=" + std::to_wstring(cr));
            return false;
        }

        cr = CM_Reenumerate_DevNode(root, 0);
        if (cr != CR_SUCCESS) {
            SetDetail(detail, L"CM_Reenumerate_DevNode(root) failed: cr=" + std::to_wstring(cr));
            return false;
        }

        SetDetail(detail, L"root rescan requested");
        return true;
    }

    bool EjectDeviceNode(DEVINST devInst, std::wstring* detail)
    {
        // disable이 거부되는 장치에 대한 fallback.
        // 장치나 드라이버가 veto하면 실패 사유를 detail에 남긴다.
        PNP_VETO_TYPE vetoType = PNP_VetoTypeUnknown;
        wchar_t vetoName[MAX_PATH]{};

        const CONFIGRET cr = CM_Request_Device_EjectW(
            devInst,
            &vetoType,
            vetoName,
            _countof(vetoName),
            0);

        if (cr != CR_SUCCESS) {
            std::wstring message = L"CM_Request_Device_EjectW failed: cr=" + std::to_wstring(cr);
            if (vetoName[0] != L'\0') {
                message += L", veto=";
                message += vetoName;
            }
            SetDetail(detail, message);
            return false;
        }

        SetDetail(detail, L"device ejected");
        return true;
    }
}

namespace DeviceBlocker
{
    BlockResult BlockByDevicePath(const std::wstring& devicePath, std::wstring* detail)
    {
        if (devicePath.empty()) {
            SetDetail(detail, L"empty device path");
            return BlockResult::NotFound;
        }

        DeviceFinder::ResolvedDevice resolved;
        if (!DeviceFinder::ResolveDeviceFromInterfacePath(devicePath, resolved, detail)) {
            DeviceFinder::DestroyResolvedDevice(resolved);
            return BlockResult::NotFound;
        }

        const std::vector<DeviceFinder::DevNodeCandidate> candidates =
            DeviceFinder::CollectBlockCandidates(resolved.devInfo.DevInst);

        std::wstring disableDetail;
        // VID/PID가 붙은 실제 장치 노드부터 disable한다.
        for (const auto& candidate : candidates) {
            if (!DeviceFinder::IsVidPidNode(candidate.instanceId)) {
                continue;
            }

            std::wstring candidateDetail;
            if (DisableByInstanceId(candidate.instanceId, &candidateDetail)) {
                DeviceFinder::DestroyResolvedDevice(resolved);
                SetDetail(detail, L"disabled " + candidate.instanceId);
                Logger::Add(L"[Block] Blacklisted HID disabled: " + candidate.instanceId);
                return BlockResult::Disabled;
            }

            if (!disableDetail.empty()) {
                disableDetail += L"; ";
            }
            disableDetail += candidate.instanceId + L" => " + candidateDetail;
        }

        // VID/PID 부모를 못 찾은 특수 케이스에서는 최초 HID 노드라도 disable해 본다.
        if (disableDetail.empty() && DisableDeviceNode(resolved.deviceInfoSet, resolved.devInfo, &disableDetail)) {
            DeviceFinder::DestroyResolvedDevice(resolved);
            SetDetail(detail, disableDetail);
            Logger::Add(L"[Block] Blacklisted HID disabled.");
            return BlockResult::Disabled;
        }

        std::wstring ejectDetail;
        // disable이 실패하면 같은 후보들에 대해 eject를 시도한다.
        for (const auto& candidate : candidates) {
            if (!DeviceFinder::IsVidPidNode(candidate.instanceId)) {
                continue;
            }

            std::wstring candidateDetail;
            if (EjectDeviceNode(candidate.devInst, &candidateDetail)) {
                DeviceFinder::DestroyResolvedDevice(resolved);
                SetDetail(detail, L"disable failed (" + disableDetail + L"), ejected " + candidate.instanceId);
                Logger::Add(L"[Block] Blacklisted HID ejected: " + candidate.instanceId);
                return BlockResult::Ejected;
            }

            if (!ejectDetail.empty()) {
                ejectDetail += L"; ";
            }
            ejectDetail += candidate.instanceId + L" => " + candidateDetail;
        }

        DeviceFinder::DestroyResolvedDevice(resolved);
        SetDetail(detail, L"disable failed (" + disableDetail + L"), eject failed (" + ejectDetail + L")");
        Logger::Add(L"[Block] Failed to disable/eject blacklisted HID.");
        return BlockResult::Failed;
    }

    EnableResult EnableByVidPid(uint16_t vid, uint16_t pid, std::wstring* detail)
    {
        const std::vector<std::wstring> instanceIds = DeviceFinder::FindVidPidInstanceIds(vid, pid);
        if (instanceIds.empty()) {
            SetDetail(detail, L"no matching VID/PID devnodes found");
            return EnableResult::NotFound;
        }

        bool anyEnabled = false;
        bool anyRemoved = false;
        std::wstring allDetails;
        for (const auto& instanceId : instanceIds) {
            std::wstring itemDetail;
            if (EnableByInstanceId(instanceId, &itemDetail)) {
                anyEnabled = true;
                Logger::Add(L"[Block] Re-enabled device: " + instanceId);
            }
            else {
                std::wstring removeDetail;
                if (RemoveByInstanceId(instanceId, &removeDetail)) {
                    anyRemoved = true;
                    Logger::Add(L"[Block] Removed disabled device for rescan: " + instanceId);
                    itemDetail += L", fallback remove: " + removeDetail;
                }
            }

            if (!allDetails.empty()) {
                allDetails += L"; ";
            }
            allDetails += instanceId + L" => " + itemDetail;
        }

        if (anyEnabled || anyRemoved) {
            std::wstring rescanDetail;
            RescanRoot(&rescanDetail);
            allDetails += L"; rescan => " + rescanDetail;
        }

        SetDetail(detail, allDetails);
        return (anyEnabled || anyRemoved) ? EnableResult::Enabled : EnableResult::Failed;
    }
}
