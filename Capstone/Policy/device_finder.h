#pragma once

#include <windows.h>
#include <cfgmgr32.h>
#include <setupapi.h>

#include <string>
#include <vector>

namespace DeviceFinder
{
    // SetupAPI에서 찾은 장치 정보 집합과 실제 devnode 정보.
    // HDEVINFO는 반드시 사용 후 DestroyResolvedDevice로 해제해야 한다.
    struct ResolvedDevice
    {
        HDEVINFO deviceInfoSet = INVALID_HANDLE_VALUE;
        SP_DEVINFO_DATA devInfo{};
    };

    // 차단 후보 devnode 하나.
    // devInst는 ConfigManager API용 핸들이고, instanceId는 로그/재오픈용 식별자다.
    struct DevNodeCandidate
    {
        DEVINST devInst = 0;
        std::wstring instanceId;
    };

    // GetLastError 값을 사람이 읽을 수 있는 로그 문자열로 변환한다.
    std::wstring FormatWin32Error(DWORD error);

    void DestroyResolvedDevice(ResolvedDevice& resolved);

    bool ResolveDeviceFromInterfacePath(
        const std::wstring& devicePath,
        ResolvedDevice& resolved,
        std::wstring* detail = nullptr);

    bool IsVidPidNode(const std::wstring& instanceId);

    std::vector<DevNodeCandidate> CollectBlockCandidates(DEVINST startDevInst);

    std::vector<std::wstring> FindVidPidInstanceIds(uint16_t vid, uint16_t pid);
}
