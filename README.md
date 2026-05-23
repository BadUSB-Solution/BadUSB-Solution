# BadUSB Solution

BadUSB 공격을 감지하고 차단하는 Windows 데스크톱 애플리케이션.

## 프로젝트 구조

```
Capstone/
├── main.cpp
├── Capstone.vcxproj / .filters / .user
│
├── Core/
│   ├── framework.h, targetver.h, Capstone.h, resource.h
│   ├── ui_message_id.h
│   ├── usb_item.h
│   └── matchingDB.db                    (post-build로 출력 폴더에 복사)
│
├── Logger/
│   └── logger.h / logger.cpp            (인메모리 링 버퍼 + UI PostMessage)
│
├── Storage/
│   └── app_paths.h / app_paths.cpp      (%APPDATA%\AbangAll\ 경로 + hidden 헬퍼)
│
├── Policy/
│   ├── policy.h / policy.cpp            (allow_list.json 화이트/블랙리스트)
│   ├── settings.h / settings.cpp        (settings.json 사용자 환경설정)
│   ├── device_blocker.h / device_blocker.cpp
│   └── device_finder.h / device_finder.cpp
│
├── Detect/
│   ├── hid_monitor.h / hid_monitor.cpp
│   ├── hid_fingerprint.h / hid_fingerprint.cpp
│   ├── hid_scan.h / hid_scan.cpp
│   └── hid_approval_flow.h / hid_approval_flow.cpp
│
├── Hook/
│   ├── badusb.h
│   └── keyboard_hook.h / keyboard_hook.cpp
│
├── Algorithm/
│   ├── PreAnalysis/
│   │   ├── KeyDecoder/
│   │   │   └── key_decoder.h / key_decoder.cpp
│   │   ├── KeyNormalizer/
│   │   │   ├── key_normalizer.h / key_normalizer.cpp
│   │   │   └── keyboard_shortcuts_ref.h
│   │   ├── ShellDetector/
│   │   │   └── shell_detector.h / shell_detector.cpp
│   │   └── UrlExtractor/
│   │       └── url_extractor.h / url_extractor.cpp
│   │
│   └── Analysis/
│       ├── TypingAnalyzer/
│       │   └── typing_analyzer.h / typing_analyzer.cpp
│       ├── ThreatMatcher/
│       │   ├── threat_matcher.h / threat_matcher.cpp
│       │   ├── threat_matcher_internal.h
│       │   ├── threat_db_connect.h / threat_db_connect.cpp
│       │   ├── threat_command_analyzer.cpp
│       │   ├── threat_executor_analyzer.cpp
│       │   └── threat_result_formatter.cpp
│       ├── UrlClassifier/
│       │   ├── url_classifier.h / url_classifier.cpp
│       │   ├── url_parser.h / url_parser.cpp
│       │   ├── entropy_filter.h / .cpp
│       │   ├── executable_filter.h / .cpp
│       │   ├── hosting_filter.h / .cpp
│       │   ├── ip_filter.h / .cpp
│       │   ├── port_filter.h / .cpp
│       │   ├── scheme_filter.h / .cpp
│       │   └── shortener_filter.h / .cpp
│       └── CompoundScorer/
│           ├── compound_scorer.h / compound_scorer.cpp
│           └── hotkey_scorer.h / hotkey_scorer.cpp
│
├── UI/
│   ├── ui_tray.h / ui_tray.cpp
│   ├── ui_tabs.h / ui_tabs.cpp
│   ├── ui_approval.h / ui_approval.cpp
│   ├── ui_buffer_view.h / ui_buffer_view.cpp
│   ├── ui_policy_tab.h / ui_policy_tab.cpp
│   ├── ui_settings_tab.h / ui_settings_tab.cpp
│   └── ui_compound_scorer.h / ui_compound_scorer.cpp
│
├── Resource/
│   ├── Capstone.rc / Capstone.aps
│   ├── ui_approval.rc / ui_approval.aps
│   ├── Capstone.ico
│   └── small.ico
│
└── ThirdParty/
    ├── sqlite3.h
    └── sqlite3.c
```

## 폴더 설명

| 폴더 | 설명 |
|------|------|
| Core | 공용 헤더(framework/targetver/resource/ui_message_id), UsbItem 데이터 모델, matchingDB.db |
| Logger | 인메모리 링 버퍼(최대 500줄) + UI에 PostMessage 알림 |
| Storage | `%APPDATA%\AbangAll\` 경로 헬퍼 + 파일 hidden 속성 처리 |
| Policy | 화이트/블랙리스트(allow_list.json), 사용자 환경설정(settings.json), Windows 장치 disable/enable |
| Detect | USB HID 장치 감지, 핑거프린트 생성, 초기 스캔, 승인 흐름 통합 |
| Hook | WH_KEYBOARD_LL 기반 키보드 입력 후킹 (전체 차단 + 버퍼링) |
| Algorithm | BadUSB 탐지 알고리즘 — PreAnalysis(키 디코드/정규화/쉘 감지/URL 추출) + Analysis(타이핑/위협 매칭/URL 분류/복합 점수) |
| UI | 트레이 아이콘, 탭(Status/Policy/Settings/Logs), 승인 다이얼로그, 키입력 버퍼 뷰, 복합 점수 뷰 |
| Resource | rc 파일, 아이콘 등 리소스 |
| ThirdParty | 외부 의존성 (sqlite3 amalgamation) |

## 사용자 데이터 (런타임 생성)

`%APPDATA%\AbangAll\` 아래에 저장 (모두 hidden 처리):

- `allow_list.json` — `Policy::Load/Save` (화이트/블랙리스트)
- `settings.json` — `Policy::LoadSettings/SaveSettings` (사용자 환경설정)
- `matchingDB.db` — (계획) `ThreatMatcher::EnsureDatabaseInstalled` 가 첫 실행 시 exe 옆에서 이동
