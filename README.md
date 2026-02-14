# 간단 설명

BadUSB 형태의 비정상 키보드 입력을 감지하고 차단하기 위한 Windows 기반 C++ 프로젝트

- **깃 클론으로 가져가도 실행시키지 못할 가능성이 높음. (IMGUI 설정)**

- 현재는 소스 코드만 예시용으로 올린다는 목적.

## 프로젝트 목적

- 시스템 전역 키 입력을 감시
- 입력 속도(CPS), 입력 시퀀스, 위험 키워드를 분석
- 위험 점수에 따라 허용/경고/차단을 결정

## 코드 구조

```text
BadUSBGuardian/
├─ src/
│  ├─ main.cpp
│  ├─ core/
│  │  ├─ KeyboardHook.h/.cpp
│  │  ├─ InputAnalyzer.h/.cpp
│  │  ├─ DefenseEngine.h/.cpp
│  │  ├─ Config.h/.cpp
│  │  └─ Logger.h/.cpp
│  └─ gui/
│     └─ MainWindow.h/.cpp
├─ tests/
│  └─ DefenseEngineTests.cpp
├─ CMakeLists.txt
└─ config.json
```

## 파일별 역할

### `src/main.cpp`

- 프로그램 시작점
- 설정 로드, 엔진/훅/UI 초기화
- 입력 이벤트 처리 루프 운영

### `src/core/KeyboardHook.*`

- Win32 저수준 키보드 훅(`WH_KEYBOARD_LL`) 등록/해제
- 키 이벤트를 `InputEvent`로 변환
- 엔진 판단 결과에 따라 입력 차단

### `src/core/InputAnalyzer.*`

- 최근 입력 버퍼 관리
- CPS 계산
- 시퀀스/키워드/주입 플래그 기반 위험 점수 계산

### `src/core/DefenseEngine.*`

- `InputAnalyzer` 결과를 최종 의사결정으로 변환
- 경고/차단 임계값 적용
- 스레드 안전 처리(`std::mutex`)

### `src/core/Config.*`

- `config.json` 로드/저장
- 임계값 및 키워드 목록 관리

### `src/core/Logger.*`

- 파일 로그 기록
- 로그 레벨(`Info`, `Warning`, `Critical`) 관리

### `src/gui/MainWindow.*`

- ImGui 기반 대시보드 UI
- CPS/점수/차단 상태/로그 표시
- UI에서 설정 변경 요청 처리

### `tests/DefenseEngineTests.cpp`

- 공격/정상 입력 시나리오 테스트
- 탐지 규칙 동작 검증

## 설정값 (`config.json`)

- `warn_cps`: 경고 CPS 임계값
- `block_cps`: 차단 CPS 임계값
- `warn_score`: 경고 점수 임계값
- `block_score`: 차단 점수 임계값
- `keywords`: 위험 키워드 목록

## 참고

- `imgui/`는 UI 라이브러리(서드파티) 코드 <- 따로 설정 필요
- 프로젝트 핵심 로직은 `src/`에 있다.
