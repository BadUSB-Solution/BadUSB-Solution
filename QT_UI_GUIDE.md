# Qt UI 실행 및 개발 가이드

작성일: 2026-05-10

## 개요

이 문서는 BadUSB Solution의 Qt Widgets 기반 UI(`CapstoneQtApp`)를 빌드하고 실행하는 방법, 현재 백엔드 연결 상태, 개발 시 주의할 점을 정리한다.

Qt UI는 기존 Win32 UI와 별도의 실행 파일로 구성되어 있다.

- 기존 Win32 앱: `Capstone.exe`
- Qt UI 앱: `CapstoneQtApp.exe`

## 요구 환경

소스 코드에서 직접 빌드하려면 아래 환경이 필요하다.

- Windows
- Visual Studio 2022 C++ Build Tools 또는 Visual Studio 2022
- CMake 3.21 이상
- Qt 6.11.0 MSVC 2022 64-bit

Qt 설치 경로가 다르면 빌드 명령의 `Qt6_DIR` 경로를 환경에 맞게 수정해야 한다.

## 개발 환경 설치

### 1. Visual Studio C++ 빌드 도구

MSVC 컴파일러와 Windows SDK가 필요하다.

설치 방법 중 하나를 선택한다.

#### 방법 A: winget 사용

```powershell
winget install --id Microsoft.VisualStudio.2022.BuildTools -e
```

설치 화면에서 아래 workload 또는 개별 구성요소를 선택한다.

- Desktop development with C++
- MSVC v143 C++ build tools
- Windows 10/11 SDK
- C++ CMake tools for Windows

#### 방법 B: 공식 페이지에서 설치

- Visual Studio / Build Tools 다운로드: https://visualstudio.microsoft.com/downloads/
- Windows 개발 도구: https://developer.microsoft.com/windows/downloads/

### 2. CMake

#### winget 사용

```powershell
winget install --id Kitware.CMake -e
```

설치 후 새 PowerShell을 열고 확인한다.

```powershell
cmake --version
```

공식 다운로드:

- https://cmake.org/download/

### 3. Qt

Qt Online Installer로 설치한다.

공식 다운로드:

- https://download.qt.io/official_releases/online_installers/

Windows x64 환경에서는 `qt-online-installer-windows-x64-online.exe`를 사용한다.

설치 시 아래 구성요소를 선택한다.

- Qt 6.11.0
- MSVC 2022 64-bit
- Qt Widgets
- Qt SVG

설치 후 기본 경로 예시:

```text
C:\Qt\6.11.0\msvc2022_64
```

다른 경로에 설치했다면 빌드 명령의 `Qt6_DIR`을 해당 경로로 수정한다.

예시:

```powershell
-DQt6_DIR="D:\Qt\6.11.0\msvc2022_64\lib\cmake\Qt6"
```

## Qt UI 빌드 방법

아래 명령은 저장소 루트에서 실행한다.

```powershell
cmake -S . -B build-qt -DBUILD_QT_UI=ON -DQt6_DIR="C:\Qt\6.11.0\msvc2022_64\lib\cmake\Qt6"
cmake --build build-qt --config Release
```

## Qt UI 실행 방법

```powershell
.\build-qt\Release\CapstoneQtApp.exe
```

빌드 시 `Capstone/Core/matchingDB.db`는 실행 폴더로 자동 복사된다.

```text
build-qt/Release/matchingDB.db
```

## 기존 Win32 앱 실행 방법

Visual Studio 솔루션 빌드 후 아래 실행 파일을 사용한다.

```powershell
.\x64\Debug\Capstone.exe
```

## 배포용 ZIP 만들기

Qt가 설치되지 않은 PC에서 바로 실행하게 하려면 `CapstoneQtApp.exe`만 보내면 안 된다. Qt DLL, platform plugin, DB 파일까지 함께 묶어야 한다.

Release 빌드 후 아래 명령을 실행한다.

```powershell
C:\Qt\6.11.0\msvc2022_64\bin\windeployqt.exe .\build-qt\Release\CapstoneQtApp.exe
copy .\Capstone\Core\matchingDB.db .\build-qt\Release\matchingDB.db
```

그 다음 `build-qt/Release` 폴더를 압축한다.

예시:

```powershell
Compress-Archive -Path .\build-qt\Release -DestinationPath .\build-qt\Abang-All-Qt-Release.zip -Force
```

배포 ZIP 안에는 최소한 아래 파일이 포함되어야 한다.

- `Release/CapstoneQtApp.exe`
- `Release/Qt6Core.dll`
- `Release/Qt6Gui.dll`
- `Release/Qt6Widgets.dll`
- `Release/platforms/qwindows.dll`
- `Release/matchingDB.db`

## 주요 Qt UI 파일

- `CMakeLists.txt`
- `Capstone/UIQt/main_qt.cpp`
- `Capstone/UIQt/MainWindow.h`
- `Capstone/UIQt/MainWindow.cpp`
- `Capstone/UIQt/Theme.h`
- `Capstone/UIQt/TrayWidget.h`
- `Capstone/UIQt/TrayWidget.cpp`
- `Capstone/UIQt/Assets/ui.qrc`
- `Capstone/UIQt/Assets/icons/*.svg`

## CMake 구성

`CMakeLists.txt`는 `CapstoneCore` 정적 라이브러리와 Qt 실행 파일 `CapstoneQtApp`을 구성한다.

Qt UI 빌드는 아래 옵션으로 켠다.

```powershell
-DBUILD_QT_UI=ON
```

`CapstoneCore`에 포함되는 주요 모듈:

- `Capstone/Core/logger.cpp`
- `Capstone/Core/policy.cpp`
- `Capstone/Hook/keyboard_hook.cpp`
- `Capstone/Detect/hid_scan.cpp`
- `Capstone/Detect/hid_fingerprint.cpp`
- `Capstone/Algorithm/PreAnalysis/key_decoder.cpp`
- `Capstone/Algorithm/PreAnalysis/key_normalizer.cpp`
- `Capstone/Algorithm/PreAnalysis/url_extractor.cpp`
- `Capstone/Algorithm/PreAnalysis/typing_analyzer.cpp`
- `Capstone/Algorithm/Analysis/ShellDetector/shell_detector.cpp`
- `Capstone/Algorithm/Analysis/ThreatMatcher/threat_matcher.cpp`
- `Capstone/Algorithm/Analysis/ThreatMatcher/threat_db_connect.cpp`
- `Capstone/Algorithm/Analysis/UrlClassifier/url_classifier.cpp`
- `Capstone/Algorithm/Analysis/CompoundScorer/compound_scorer.cpp`
- `Capstone/Algorithm/Analysis/CompoundScorer/hotkey_scorer.cpp`
- `Capstone/ThirdParty/sqlite3.c`

## 현재 백엔드 연결 상태

| 화면/기능 | 연결 상태 |
|------|------|
| USB 목록 | `HidScan`, `HidFingerprint`, `Policy` 기반 실제 HID 장치 표시 |
| 장치 허용/차단 | `Policy::AddWhitelist`, `Policy::AddBlacklist`, `Policy::Remove` 연결 |
| 로그 | `Logger::GetAll`, `Logger::SetNotifyWindow` 기반 실시간 append |
| USB 삽입/해제 | `RegisterDeviceNotificationW`, `WM_DEVICECHANGE` 연결 |
| 버퍼 모니터 | `KeyboardHook::GetBufferedKeys`, `KeyDecoder::Decode` 연결 |
| 행위 분석 | `ShellDetector`, `ThreatMatcher`, `HotkeyScorer`, `CompoundScorer` 기반 위협 점수 추이 연결 |
| 설정 | `QSettings` 기반 미등록 장치 처리 정책/팝업 알림 저장 |

## 현재 한계

### KeyboardHook

현재 `KeyboardHook`은 Windows low-level keyboard hook 기반이다.

- BadUSB 장치 입력만 골라서 잡는 구조가 아니다.
- hook이 활성화되면 일반 키보드 입력도 같이 버퍼에 들어올 수 있다.
- hook이 활성화된 동안 전체 키보드 입력이 차단될 수 있다.

장치별 정밀 차단은 Raw Input 또는 HID handle 기반 필터링 구조가 추가로 필요하다.

### 입력 버퍼/정규화

입력 버퍼에 일부 키가 이상하게 보이거나 정규화가 기대와 다르게 나오는 문제는 UI보다 아래 계층에서 발생할 가능성이 높다.

- 원본 입력 버퍼부터 이상함: `KeyboardHook` / VK / scanCode 처리 문제
- 원본은 괜찮고 정규화만 이상함: `KeyNormalizer` / `KeyDecoder` 문제
- 정규화는 괜찮고 점수만 이상함: `ThreatMatcher` / DB / `CompoundScorer` 문제

### 행위 분석

- `ThreatMatcher`, `HotkeyScorer`, `CompoundScorer` 기반 위협 점수는 연결되어 있다.
- `TypingAnalyzer`는 아직 실질 구현이 부족하다.
- 입력속도 기반 완전한 행위 분석 차트는 추가 구현이 필요하다.
- URL 분석도 현재 완전 구현 상태는 아니다.
