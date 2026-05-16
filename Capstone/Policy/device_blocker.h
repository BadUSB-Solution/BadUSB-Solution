#pragma once

#include <cstdint>
#include <string>

namespace DeviceBlocker
{
    // Windows 장치 상태 변경 결과.
    // Disabled는 장치 관리자에서 "사용 안 함"으로 바뀐 상태를 의미한다.
    enum class BlockResult
    {
        Disabled,
        Ejected,
        NotFound,
        Failed
    };

    enum class EnableResult
    {
        Enabled,
        NotFound,
        Failed
    };

    // HID interface path를 받아 실제 VID/PID 장치 노드를 찾아 disable/eject 한다.
    // 장치 상태 변경은 관리자 권한이 필요할 수 있다.
    BlockResult BlockByDevicePath(const std::wstring& devicePath, std::wstring* detail = nullptr);

    // DB에 남아 있는 VID/PID만으로 Windows devnode를 찾아 다시 enable 한다.
    // 이미 disable된 장치는 현재 HID DevicePath가 사라질 수 있어 VID/PID 검색이 더 안정적이다.
    EnableResult EnableByVidPid(uint16_t vid, uint16_t pid, std::wstring* detail = nullptr);
}
