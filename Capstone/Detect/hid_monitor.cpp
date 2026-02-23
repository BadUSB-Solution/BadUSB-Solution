// hid 장비 모니터링 부분

//#include "hid_monitor.h"
//
//#include <dbt.h>
//#include <initguid.h>
//#include <hidclass.h>
//
//#include <string>
//#include <unordered_set>
//#include <sstream>
//#include <cstdint>
//
//#include "logger.h"
//#include "policy.h"
//#include "ui_approval.h"
//
//namespace HidMonitor
//{
//    static HDEVNOTIFY g_hDevNotify = nullptr;
//
//    // 중복 제거
//    static std::unordered_set<std::wstring> g_seenDevicePaths;
//    static std::unordered_set<uint64_t> g_seenVidPid; // (vid<<16)|pid
//
//    // 쿨다운
//    static ULONGLONG g_lastArrivalTick = 0;
//    static const ULONGLONG ARRIVAL_COOLDOWN_MS = 300;
//
//    static std::wstring HexU16(uint16_t v)
//    {
//        std::wstringstream ss;
//        ss << L"0x" << std::hex << v << std::dec;
//        return ss.str();
//    }
//
//    // path에서 vid, pid 추출 부분 
//    static bool ExtractVidPid(const std::wstring& path, uint16_t& vid, uint16_t& pid)
//    {
//        size_t v = path.find(L"VID_");
//        size_t p = path.find(L"PID_");
//
//        if (v == std::wstring::npos || p == std::wstring::npos) return false;
//        if (v + 8 > path.size() || p + 8 > path.size()) return false;
//
//        vid = (uint16_t)wcstoul(path.substr(v + 4, 4).c_str(), nullptr, 16);
//        pid = (uint16_t)wcstoul(path.substr(p + 4, 4).c_str(), nullptr, 16);
//        return true;
//    }
//
//    static void HandleAutoHidArrival(HWND hWnd, const std::wstring& devicePath)
//    {
//        uint16_t vid = 0, pid = 0;
//        bool ok = ExtractVidPid(devicePath, vid, pid);
//
//        // fingerprint: VID/PID 기반
//        uint64_t fingerprint = 0;
//        if (ok) {
//            fingerprint = ((uint64_t)vid << 16) | (uint64_t)pid;
//        }
//        else {
//            // fallback hash
//            uint64_t h = 1469598103934665603ULL;
//            for (wchar_t c : devicePath) { h ^= (uint64_t)c; h *= 1099511628211ULL; }
//            fingerprint = h;
//        }
//
//
//        // 로그 부분
//        Logger::Add(L"----------------------------------------");
//        Logger::Add(L"[D] USB HID Device Connected? -> Yes");
//
//        if (ok) {
//            Logger::Add(L"[E] VID=" + HexU16(vid) + L" PID=" + HexU16(pid));
//            Logger::Add(L"[E] Fingerprint (VID/PID) = " + std::to_wstring(fingerprint));
//        }
//        else {
//            Logger::Add(L"[E] VID/PID parse failed. Fingerprint (fallback) = " + std::to_wstring(fingerprint));
//        }
//
//        Logger::Add(L"[F] Default Policy: HOLD");
//        Logger::Add(L"[G] Notify UserMode Service");
//        Logger::Add(L"[H] Service Collects Device Info");
//
//        if (Policy::IsWhitelisted(fingerprint)) {
//            Logger::Add(L"[I] In Whitelist -> [J] ALLOW");
//            Logger::Add(L"[T] Policy unchanged");
//            return;
//        }
//
//        if (Policy::IsBlacklisted(fingerprint)) {
//            Logger::Add(L"[K] In Blacklist -> [L] BLOCK");
//            Logger::Add(L"[T] Policy unchanged");
//            return;
//        }
//
//        Logger::Add(L"[M] Observation Window");
//        Sleep(300);
//
//        Logger::Add(L"[N] Behavior Analysis");
//        Logger::Add(L"[O] Risk Low");
//
//        Logger::Add(L"[P] User Approval Request");
//        bool allow = ApprovalUI::AskUserApproval(hWnd, vid, pid, fingerprint);
//
//        if (allow) {
//            Logger::Add(L"[Q] User Allow -> [J] ALLOW");
//            Policy::AddWhitelist(fingerprint);
//            Logger::Add(L"[T] Added to Whitelist");
//        }
//        else {
//            Logger::Add(L"[Q] User Deny -> [L] BLOCK");
//            Policy::AddBlacklist(fingerprint);
//            Logger::Add(L"[T] Added to Blacklist");
//        }
//    }
//
//    // 기기 정보 등록
//    void RegisterNotifications(HWND hWnd)
//    {
//        DEV_BROADCAST_DEVICEINTERFACE_W filter{};
//        filter.dbcc_size = sizeof(filter);
//        filter.dbcc_devicetype = DBT_DEVTYP_DEVICEINTERFACE;
//        filter.dbcc_classguid = GUID_DEVINTERFACE_HID;
//
//        g_hDevNotify = RegisterDeviceNotificationW(hWnd, &filter, DEVICE_NOTIFY_WINDOW_HANDLE);
//
//        Logger::Add(g_hDevNotify
//            ? L"[Init] HID notifications registered."
//            : L"[Init] HID notification registration failed.");
//    }
//
//    LRESULT OnDeviceChange(HWND hWnd, WPARAM wParam, LPARAM lParam)
//    {
//        if (wParam == DBT_DEVICEARRIVAL && lParam) {
//            auto* hdr = (PDEV_BROADCAST_HDR)lParam;
//            if (hdr->dbch_devicetype != DBT_DEVTYP_DEVICEINTERFACE)
//                return 0;
//
//            auto* di = (PDEV_BROADCAST_DEVICEINTERFACE_W)lParam;
//            std::wstring path = di->dbcc_name ? di->dbcc_name : L"";
//
//            // 쿨다운
//            ULONGLONG now = GetTickCount64();
//            if (now - g_lastArrivalTick < ARRIVAL_COOLDOWN_MS)
//                return 0;
//            g_lastArrivalTick = now;
//
//            // DevicePath 중복 제거
//            if (!path.empty() && !g_seenDevicePaths.insert(path).second) {
//                Logger::Add(L"[Auto] Duplicate arrival ignored (same DevicePath).");
//                return 0;
//            }
//
//            // VID, PID 중복 제거
//            uint16_t vid = 0, pid = 0;
//            if (ExtractVidPid(path, vid, pid)) {
//                uint64_t key = ((uint64_t)vid << 16) | (uint64_t)pid;
//                if (!g_seenVidPid.insert(key).second) {
//                    Logger::Add(L"[Auto] Duplicate arrival ignored (same VID/PID).");
//                    return 0;
//                }
//            }
//
//            HandleAutoHidArrival(hWnd, path);
//            return 0;
//        }
//
//        if (wParam == DBT_DEVICEREMOVECOMPLETE && lParam) {
//            auto* hdr = (PDEV_BROADCAST_HDR)lParam;
//            if (hdr->dbch_devicetype != DBT_DEVTYP_DEVICEINTERFACE)
//                return 0;
//
//            auto* di = (PDEV_BROADCAST_DEVICEINTERFACE_W)lParam;
//            std::wstring path = di->dbcc_name ? di->dbcc_name : L"";
//            if (!path.empty()) g_seenDevicePaths.erase(path);
//
//            uint16_t vid = 0, pid = 0;
//            if (ExtractVidPid(path, vid, pid)) {
//                uint64_t key = ((uint64_t)vid << 16) | (uint64_t)pid;
//                g_seenVidPid.erase(key);
//            }
//            return 0;
//        }
//
//        return -1;
//    }
//}

#include "hid_monitor.h"
#include "../hid_approval_flow.h" 

#include <dbt.h>
#include <initguid.h>
#include <hidclass.h>

#include <string>
#include <unordered_set>

#include "logger.h"

namespace HidMonitor
{
    static HDEVNOTIFY g_hDevNotify = nullptr;

    // DevicePath 중복 제거 (이벤트 중복 방지)
    static std::unordered_set<std::wstring> g_seenDevicePaths;

    // 쿨다운
    static ULONGLONG g_lastArrivalTick = 0;
    static const ULONGLONG ARRIVAL_COOLDOWN_MS = 300;

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

            // 쿨다운
            ULONGLONG now = GetTickCount64();
            if (now - g_lastArrivalTick < ARRIVAL_COOLDOWN_MS)
                return 0;
            g_lastArrivalTick = now;

            // 중복 제거
            if (!path.empty() && !g_seenDevicePaths.insert(path).second) {
                Logger::Add(L"[Auto] Duplicate arrival ignored (same DevicePath).");
                return 0;
            }

            Logger::Add(L"[Auto] HID arrival detected.");
            DeviceApprovalFlow::ApproveFromPath(hWnd, path);
            return 0;
        }

        if (wParam == DBT_DEVICEREMOVECOMPLETE && lParam) {
            auto* hdr = (PDEV_BROADCAST_HDR)lParam;
            if (hdr->dbch_devicetype != DBT_DEVTYP_DEVICEINTERFACE)
                return 0;

            auto* di = (PDEV_BROADCAST_DEVICEINTERFACE_W)lParam;
            std::wstring path = di->dbcc_name ? di->dbcc_name : L"";
            if (!path.empty()) g_seenDevicePaths.erase(path);

            Logger::Add(L"[Auto] HID removed.");
            return 0;
        }

        return -1;
    }
}