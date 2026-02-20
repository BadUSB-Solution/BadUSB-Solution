#pragma once
#include <Windows.h>
#include <stdint.h>
#include <winioctl.h>

#ifndef BADUSB_WIN32_PATH
#define BADUSB_WIN32_PATH       L"\\\\.\\BadUsbFilter"
#endif

#define IOCTL_BADUSB_GET_EVENT          CTL_CODE(FILE_DEVICE_UNKNOWN, 0x801, METHOD_BUFFERED, FILE_READ_DATA)
#define IOCTL_BADUSB_SET_DECISION       CTL_CODE(FILE_DEVICE_UNKNOWN, 0x802, METHOD_BUFFERED, FILE_WRITE_DATA)

enum class BadUsbEventType : uint32_t {
    None = 0,
    DeviceConnected = 1,
};

enum class BadUsbDecision : uint32_t {
    Hold = 0,
    Allow = 1,
    Block = 2,
};

#pragma pack(push, 1)
struct BadUsbDeviceEvent {
    BadUsbEventType type;
    uint64_t deviceInstanceIdHash;
    uint16_t vid;
    uint16_t pid;
    uint32_t interfaceClass;
    uint32_t interfaceSubClass;
    uint32_t interfaceProtocol;
};

struct BadUsbDecisionRequest {
    uint64_t deviceInstanceIdHash;
    BadUsbDecision decision;
};
#pragma pack(pop)
