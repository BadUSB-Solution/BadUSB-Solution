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
│   ├── logger.h / logger.cpp
│   └── policy.h / policy.cpp
│
├── Detect/
│   └── hid_monitor.h / hid_monitor.cpp
│
├── Hook/
│   └── badusb.h
│
├── Algorithm/
│
├── UI/
│   ├── ui_tray.h / ui_tray.cpp
│   ├── ui_tabs.h / ui_tabs.cpp
│   └── ui_approval.h / ui_approval.cpp
│
└── Resource/
    ├── Capstone.rc
    ├── Capstone.ico
    └── small.ico
```

## 폴더 설명

| 폴더 | 설명 |
|------|------|
| Core | 공용 헤더, 로깅, 정책 관리 등 프로젝트 공통 인프라 |
| Detect | USB HID 장치 연결/해제 감지 |
| Hook | 키보드 입력 후킹 (추후 구현) |
| Algorithm | BadUSB 판별 알고리즘 (추후 구현) |
| UI | 트레이 아이콘, 탭 화면, 장치 승인 대화상자 |
| Resource | rc 파일, 아이콘 등 리소스 |
