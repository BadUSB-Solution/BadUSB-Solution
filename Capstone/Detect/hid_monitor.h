#pragma once
#include <windows.h>

namespace HidMonitor
{
    // HID 장치 도착/제거 알림 등록
    void RegisterNotifications(HWND hWnd);

    // WM_DEVICECHANGE 기반 HID 이벤트 처리
    // 처리 완료 시 0, 미처리 시 -1 반환
    LRESULT OnDeviceChange(HWND hWnd, WPARAM wParam, LPARAM lParam);

    // 차단/비활성화 후 경로를 seen 집합에서 제거 — 재연결 인식을 위해 호출
    void ForgetPath(const std::wstring& path);
}
