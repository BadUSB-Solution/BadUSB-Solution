#pragma once

// 사용자 환경설정 — %APPDATA%\AbangAll\settings.json 에 저장.
// 화이트/블랙리스트와 같은 Policy 네임스페이스에 둔다 (둘 다 사용자가 정의하는 정책).
namespace Policy
{
    // 새로운 미등록 장치가 연결됐을 때 처리 방법.
    enum class UnknownDeviceAction
    {
        Approval = 0,   // 승인 대기 (UI 팝업으로 사용자에게 묻기)
        AutoBlock = 1,  // 자동 차단
        AutoAllow = 2   // 자동 허용
    };

    // 사용자 환경설정.
    // 호출 패턴: GetSettings() 로 스냅샷 받아 수정 → SetSettings() 로 적용.
    struct Settings
    {
        // ── 일반 환경설정 ──
        bool autoStart       = false;  // Windows 시작 시 자동 실행
        bool minimizeToTray  = true;   // 창을 닫을 때 시스템 트레이로 최소화

        // ── 위험 탐지 정책 ──
        UnknownDeviceAction unknownDeviceAction = UnknownDeviceAction::Approval;
        bool behaviorMonitoring = true; // 행위 기반 모니터링 활성화

        // ── 알림 및 리포트 ──
        bool notifyOnRisk = true;       // 위험 장치 연결 시 우측 하단 팝업 알림
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
