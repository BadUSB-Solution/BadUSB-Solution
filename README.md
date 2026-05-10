# BadUSB Solution

BadUSB 공격을 감지하고 차단하는 Windows 데스크톱 애플리케이션.

## Qt UI 빠른 실행

현재 Figma 기반 UI는 Qt Widgets 실행 파일 `CapstoneQtApp.exe`로 실행한다.

### 준비 사항

- Windows
- Visual Studio 2022 C++ 빌드 도구
- CMake 3.21 이상
- Qt 6.11.0 MSVC 2022 64-bit

아래 명령은 저장소 루트에서 실행한다. Qt 설치 경로가 다르면 `Qt6_DIR`만 본인 환경에 맞게 바꾸면 된다.

### 빌드

```powershell
cmake -S . -B build-qt -DBUILD_QT_UI=ON -DQt6_DIR="C:\Qt\6.11.0\msvc2022_64\lib\cmake\Qt6"
cmake --build build-qt --config Release
```

### 실행

```powershell
.\build-qt\Release\CapstoneQtApp.exe
```

빌드 후 `Capstone\Core\matchingDB.db`는 실행 폴더로 자동 복사된다.

### 기존 Win32 앱 실행

기존 Win32 UI/트레이 앱을 실행하려면 Visual Studio 솔루션 빌드 후 아래 실행 파일을 사용한다.

```powershell
.\x64\Debug\Capstone.exe
```

## Qt UI와 백엔드 연결 상태

Qt UI는 단순 화면 mock만 있는 상태가 아니라, 현재 일부 핵심 백엔드와 연결되어 있다.

| 화면/기능 | 연결 상태 |
|------|------|
| USB 목록 | `HidScan`, `HidFingerprint`, `Policy` 기반 실제 HID 장치 표시 |
| 장치 허용/차단 | `Policy::AddWhitelist`, `Policy::AddBlacklist`, `Policy::Remove` 연결 |
| 로그 | `Logger::GetAll`, `Logger::SetNotifyWindow` 기반 실시간 append |
| USB 삽입/해제 | `RegisterDeviceNotificationW`, `WM_DEVICECHANGE` 연결 |
| 버퍼 모니터 | `KeyboardHook::GetBufferedKeys`, `KeyDecoder::Decode` 연결 |
| 행위 분석 | `ShellDetector`, `ThreatMatcher`, `HotkeyScorer`, `CompoundScorer` 기반 위협 점수 추이 연결 |
| 설정 | `QSettings` 기반 미등록 장치 처리 정책/팝업 알림 저장 |

현재 `KeyboardHook`은 Windows low-level keyboard hook 기반이라 장치별 입력만 분리해서 차단하지는 않는다. 후킹이 활성화된 동안에는 전체 키보드 입력이 버퍼링/차단될 수 있다.

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
