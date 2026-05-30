#pragma once
// ============================================================================
//  WinNotify — Windows 트레이 알림 (자체 hidden tray icon + Shell_NotifyIconW)
//
//  목적:
//   - Qt 의 QSystemTrayIcon::showMessage() 는 Windows 자체 알림음을 끄는
//     플래그(NIIF_NOSOUND) 를 노출하지 않음.
//   - "알림 소리" 사용자 설정으로 sound 를 완전히 제어하려면 raw Win32 의
//     Shell_NotifyIconW + NIIF_NOSOUND 를 직접 호출해야 함.
//
//  구현:
//   - 첫 호출 시 hidden message-only window + hidden tray icon (NIM_ADD) 생성
//   - notify() 마다 NIM_MODIFY + NIF_INFO 로 알림 발사
//   - silent=true 면 NIIF_NOSOUND 로 OS 알림음 차단
// ============================================================================

#include <QString>

namespace WinNotify
{
    // 알림 발사. silent=true 면 Windows 자체 알림음 OFF.
    // (호출자가 별도 QApplication::beep() 를 통해 소리 제어)
    void notify(const QString& title, const QString& body, bool silent, int timeoutMs);

    // 앱 종료 시 정리 — 등록한 트레이 아이콘 제거.
    void cleanup();
}
