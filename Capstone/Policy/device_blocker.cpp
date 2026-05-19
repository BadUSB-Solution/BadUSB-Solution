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

    // 장치 노드의 상태(enable/disable)를 한 번 변경한다.
    //  stateChange : DICS_ENABLE 또는 DICS_DISABLE
    //  scope       : DICS_FLAG_GLOBAL       → 모든 하드웨어 프로파일에 적용 (전역)
    //                DICS_FLAG_CONFIGSPECIFIC → 현재 하드웨어 프로파일에만 적용 (개별)
    //
    //  Windows는 두 스코프에 disable 플래그를 별도로 저장한다. 그래서
    //  GLOBAL로만 disable 했더라도 CONFIGSPECIFIC 쪽에 과거 disable 흔적이
    //  남아 있으면 GLOBAL enable만으로는 장치가 살아나지 않을 수 있다.
    //  이 함수는 "한쪽 스코프 한 번"만 처리하므로, 호출자가 필요에 따라
    //  두 번 호출하는 식으로 조합한다.
    bool ChangeDeviceNodeState(
        HDEVINFO deviceInfoSet,
        SP_DEVINFO_DATA& devInfo,
        DWORD stateChange,
        DWORD scope,
        const wchar_t* label,
        std::wstring* detail)
    {
        // SetupAPI에 전달할 "장치 상태를 바꿔달라"는 요청 파라미터.
        // 장치 관리자 GUI에서 "사용 안 함 / 사용"을 누르는 것과 같은 경로다.
        SP_PROPCHANGE_PARAMS params{};
        params.ClassInstallHeader.cbSize = sizeof(SP_CLASSINSTALL_HEADER);
        params.ClassInstallHeader.InstallFunction = DIF_PROPERTYCHANGE;
        params.StateChange = stateChange;
        params.Scope = scope;
        params.HwProfile = 0; // 0 = 현재 하드웨어 프로파일

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

        // ─────────────────────────────────────────────────────────────
        //  왜 두 번 호출하는가?
        //
        //  Windows는 장치 disable 상태를 두 군데에 따로 기록한다.
        //    1) GLOBAL       : "모든 하드웨어 프로파일에서 disable"
        //    2) CONFIGSPECIFIC: "지금 쓰는 하드웨어 프로파일에서만 disable"
        //
        //  과거에 어느 경로로든 disable이 들어갔는지 알기 어렵고,
        //  한쪽만 enable 하면 다른 쪽 플래그가 남아서 장치가 안 살아나는
        //  케이스가 실제로 있다. (MSDN의 SP_PROPCHANGE_PARAMS 문서와
        //  Microsoft devcon 샘플 모두 ENABLE은 두 스코프 다 호출하라고
        //  권장한다.)
        //
        //  그래서 GLOBAL → CONFIGSPECIFIC 순서로 한 번씩, 총 두 번 호출한다.
        //  실패 사유는 각각 따로 받아두고, 한 쪽만 성공해도 장치가 살아나는
        //  경우가 많으니 OR 결과로 성공/실패를 판정한다.
        // ─────────────────────────────────────────────────────────────

        // (1) 전역 스코프 enable
        std::wstring globalDetail;
        const bool globalOk = ChangeDeviceNodeState(
            deviceInfoSet,
            devInfo,
            DICS_ENABLE,
            DICS_FLAG_GLOBAL,
            L"enabled(global)",
            &globalDetail);

        // (2) 현재 하드웨어 프로파일 스코프 enable
        std::wstring configDetail;
        const bool configOk = ChangeDeviceNodeState(
            deviceInfoSet,
            devInfo,
            DICS_ENABLE,
            DICS_FLAG_CONFIGSPECIFIC,
            L"enabled(config)",
            &configDetail);

        SetupDiDestroyDeviceInfoList(deviceInfoSet);

        // 로그에 어느 쪽이 성공/실패했는지 둘 다 남긴다.
        // 나중에 enable이 안 풀리는 케이스 디버깅할 때 어느 스코프 문제인지
        // 바로 보일 수 있게 하기 위함이다.
        std::wstring combined = L"global=" + globalDetail + L"; config=" + configDetail;
        SetDetail(detail, combined);

        // 한 스코프만 풀려도 장치가 살아나는 경우가 있어
        // 둘 중 하나라도 성공이면 enabled로 본다.
        return globalOk || configOk;
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
