#include "hid_monitor.h"

#include <dbt.h>
#include <initguid.h>
#include <hidclass.h>

#include <string>
#include <unordered_set>

#include "hid_approval_flow.h"
#include "logger.h"
#include "json_log.h"

namespace HidMonitor
{
    static HDEVNOTIFY g_hDevNotify = nullptr;

    // 중복 도착 이벤트 방지용 DevicePath 집합
    static std::unordered_set<std::wstring> g_seenDevicePaths;

    // 연속 도착 이벤트 억제용 쿨다운 설정
    static ULONGLONG g_lastArrivalTick = 0;
    static const ULONGLONG ARRIVAL_COOLDOWN_MS = 300;

    void ForgetPath(const std::wstring& path)
    {
        if (!path.empty())
            g_seenDevicePaths.erase(path);
    }

    void RegisterNotifications(HWND hWnd)
    {
        DEV_BROADCAST_DEVICEINTERFACE_W filter{};
        filter.dbcc_size = sizeof(filter);
        filter.dbcc_devicetype = DBT_DEVTYP_DEVICEINTERFACE;
        filter.dbcc_classguid = GUID_DEVINTERFACE_HID;

        g_hDevNotify = RegisterDeviceNotificationW(hWnd, &filter, DEVICE_NOTIFY_WINDOW_HANDLE);

        Logger::Add(g_hDevNotify
            ? L"[Init] HID notifications registered."
            : L"[Init] HID notification registration failed.");
    }

    LRESULT OnDeviceChange(HWND hWnd, WPARAM wParam, LPARAM lParam)
    {
        if (wParam == DBT_DEVICEARRIVAL && lParam) {
            auto* hdr = (PDEV_BROADCAST_HDR)lParam;
            if (hdr->dbch_devicetype != DBT_DEVTYP_DEVICEINTERFACE)
                return 0;

            auto* di = (PDEV_BROADCAST_DEVICEINTERFACE_W)lParam;
            std::wstring path = di->dbcc_name ? di->dbcc_name : L"";

            const ULONGLONG now = GetTickCount64();
            if (now - g_lastArrivalTick < ARRIVAL_COOLDOWN_MS)
                return 0;
            g_lastArrivalTick = now;

            if (!path.empty() && !g_seenDevicePaths.insert(path).second) {
                Logger::Add(L"[Auto] Duplicate arrival ignored (same DevicePath).");
                return 0;
            }

            Logger::Add(L"[Auto] HID arrival detected.");
            JsonLog::Add(path, L"DEVICE_CONNECTED", L"");
            DeviceApprovalFlow::ApproveDeviceFromPath(hWnd, path);
            return 0;
        }

        if (wParam == DBT_DEVICEREMOVECOMPLETE && lParam) {
            auto* hdr = (PDEV_BROADCAST_HDR)lParam;
            if (hdr->dbch_devicetype != DBT_DEVTYP_DEVICEINTERFACE)
                return 0;

            auto* di = (PDEV_BROADCAST_DEVICEINTERFACE_W)lParam;
            std::wstring path = di->dbcc_name ? di->dbcc_name : L"";
            if (!path.empty()) {
                g_seenDevicePaths.erase(path);
            } else {
                // 프로그래밍 방식 비활성화(차단) 시 경로가 비어있을 수 있음.
                // seen 집합 전체를 초기화해 재연결 인식을 보장한다.
                g_seenDevicePaths.clear();
            }

            Logger::Add(L"[Auto] HID removed.");
            JsonLog::Add(path.empty() ? L"(unknown)" : path,
                         L"DEVICE_DISCONNECTED", L"");
            return 0;
        }

        return -1;
    }
}
