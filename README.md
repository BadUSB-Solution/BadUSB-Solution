# BadUSB Solution

BadUSB 공격을 감지하고 차단하는 Windows 데스크톱 애플리케이션.

## 프로젝트 구조

```
Capstone/
├── main.cpp
├── Capstone.vcxproj / .filters / .user
│
├── Core/
│   ├── framework.h, targetver.h, Capstone.h, Resource.h
│   ├── ui_message_id.h
│   ├── usb_item.h
│   ├── logger.h / logger.cpp
│   └── policy.h / policy.cpp
│
├── Detect/
│   ├── hid_monitor.h / hid_monitor.cpp
│   ├── hid_fingerprint.h / hid_fingerprint.cpp
│   ├── hid_initialScan.h / hid_initialScan.cpp
│   └── hid_approval_flow.h / hid_approval_flow.cpp
│
├── Hook/
│   ├── badusb.h
│   └── keyboard_hook.h / keyboard_hook.cpp
│
├── Algorithm/
│   ├── PreAnalysis/
│   │   ├── key_decoder.h / key_decoder.cpp
│   │   └── typing_analyzer.h / typing_analyzer.cpp
│   └── Analysis/
│       ├── ShellDetector/
│       │   └── shell_detector.h / shell_detector.cpp
│       ├── ThreatMatcher/
│       │   └── threat_matcher.h / threat_matcher.cpp
│       ├── UrlClassifier/
│       │   └── url_classifier.h / url_classifier.cpp
│       └── CompoundScorer/
│           └── compound_scorer.h / compound_scorer.cpp
│
├── UI/
│   ├── ui_tray.h / ui_tray.cpp
│   ├── ui_tabs.h / ui_tabs.cpp
│   ├── ui_approval.h / ui_approval.cpp
│   └── ui_buffer_view.h / ui_buffer_view.cpp
│
└── Resource/
    ├── Capstone.rc / ui_approval.rc
    ├── Capstone.ico
    └── small.ico
```

## 폴더 설명

| 폴더 | 설명 |
|------|------|
| Core | 공용 헤더, 로깅, 정책(화이트/블랙리스트) 관리 |
| Detect | USB HID 장치 감지, 핑거프린트 생성, 승인 흐름 통합 |
| Hook | WH_KEYBOARD_LL 기반 키보드 입력 후킹 (전체 차단 + 버퍼링) |
| Algorithm | BadUSB 탐지 알고리즘 (분석 전 처리 + 명령어 분석) |
| UI | 트레이 아이콘, 탭 화면, 장치 승인 다이얼로그, 키입력 버퍼 뷰어 |
| Resource | rc 파일, 아이콘 등 리소스 |
