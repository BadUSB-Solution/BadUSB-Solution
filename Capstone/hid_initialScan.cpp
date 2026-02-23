#include "hid_initialScan.h"

#include <framework.h>
#include <setupapi.h>
#include <hidclass.h>
#include <initguid.h>
#include <vector>
#include <string>

#pragma comment(lib, "Setupapi.lib")

namespace HidInitialScan
{
    std::vector<std::wstring> EnumerateHidDevicePaths()
    {
        std::vector<std::wstring> out;

        HDEVINFO h = SetupDiGetClassDevsW(
            &GUID_DEVINTERFACE_HID,
            nullptr,
            nullptr,
            DIGCF_PRESENT | DIGCF_DEVICEINTERFACE
        );
        if (h == INVALID_HANDLE_VALUE) {
            return out;
        }

        SP_DEVICE_INTERFACE_DATA ifData{};
        ifData.cbSize = sizeof(ifData);

        for (DWORD i = 0; ; i++) {
            if (!SetupDiEnumDeviceInterfaces(h, nullptr, &GUID_DEVINTERFACE_HID, i, &ifData)) {
                break;
            }

            DWORD required = 0;
            SetupDiGetDeviceInterfaceDetailW(h, &ifData, nullptr, 0, &required, nullptr);
            if (required == 0) continue;

            std::vector<BYTE> buf(required);
            auto* detail = reinterpret_cast<SP_DEVICE_INTERFACE_DETAIL_DATA_W*>(buf.data());
            detail->cbSize = sizeof(SP_DEVICE_INTERFACE_DETAIL_DATA_W);

            if (!SetupDiGetDeviceInterfaceDetailW(h, &ifData, detail, required, nullptr, nullptr)) {
                continue;
            }

            if (detail->DevicePath && detail->DevicePath[0] != L'\0') {
                out.emplace_back(detail->DevicePath);
            }
        }

        SetupDiDestroyDeviceInfoList(h);
        return out;
    }
}