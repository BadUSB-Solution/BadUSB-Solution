#pragma once

// 사용자 환경설정 — %APPDATA%\AbangAll\settings.json 에 저장.
// 화이트/블랙리스트와 같은 Policy 네임스페이스에 둔다 (둘 다 사용자가 정의하는 정책).
namespace Policy
{
    // 사용자 환경설정.
    // 호출 패턴: GetSettings() 로 스냅샷 받아 수정 → SetSettings() 로 적용.
    //
    // 테마/언어는 Qt QSettings(레지스트리)에 그대로 두고, 그 외 UI 설정은
    // 이 구조체에 모아 settings.json 한 곳에서 관리한다.
    struct Settings
    {
        // ── 일반 ──
        bool autoStart          = false;  // Windows 시작 시 자동 실행
        bool showMainOnStartup  = true;   // 시작 시 메인 창 표시 (false 면 트레이만)

        // ── 알림 ──
        bool notifyOnRisk            = true;  // 위험 장치 연결 시 우측 하단 팝업 알림
        bool notificationSound       = true;  // 알림 표시 시 시스템 비프음
        int  notificationDurationSec = 3;     // 알림 표시 시간 (3 / 5 / 10)
        int  notificationProfile     = 1;     // 알림 프리셋: 0 간결 / 1 기본 / 2 강조

        // ── 로그 ──
        int  logRetentionDays        = 30;    // 7 / 30 / 90 / 0(=영구)
    };

    // 파일 → 메모리. 파일 없으면 기본값으로 초기화 + 즉시 1회 저장.
    bool LoadSettings();

    // 현재 메모리 상태를 파일로 저장.
    bool SaveSettings();

    // 현재 설정 스냅샷.
    Settings GetSettings();

    // 새 설정으로 교체하고 자동 저장.
    bool SetSettings(const Settings& s);
}
