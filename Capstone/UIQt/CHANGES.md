# Qt UI 변경 내역

> 빌드 타겟: `BadUSB-Solution/build-qt/Debug/CapstoneQtApp.exe`
> 수정 파일: `MainWindow.cpp`, `MainWindow.h`, `Theme.h`

---

## Theme.h — 스타일 추가

기존 QSS에 아래 셀렉터 추가:

| 셀렉터 | 용도 |
|---|---|
| `QLabel#neutralBadge` | 흰 배경 회색 테두리 배지 |
| `QLabel#blockBadge` | 빨간 배경 차단 배지 |
| `QLabel#sectionHeading` | 섹션 제목 레이블 |
| `QLabel#systemInfoLabel` | 시스템 정보 키 레이블 |
| `QLabel#systemInfoValue` | 시스템 정보 값 레이블 |
| `QPushButton#settingsTab` | 설정 탭 버튼 (비활성) |
| `QPushButton#settingsTab[active="true"]` | 설정 탭 버튼 (활성, 주황 underline) |
| `QWidget#settingsTabBar` | 설정 탭 바 배경 |
| `QCheckBox#settingsCheck` | 설정 체크박스 |
| `QCheckBox#settingsCheck::indicator(:checked)` | 체크박스 인디케이터 (주황) |
| `QLineEdit#searchInput` | 검색 입력 필드 |
| `QFrame#divider` | 수평 구분선 (1px, `#E2E8F0`) |

---

## MainWindow.h — 메서드 선언 추가

```cpp
QWidget* createHelpPage();   // 도움말 페이지
```

---

## MainWindow.cpp — 페이지별 변경

### 공통 헬퍼 (anonymous namespace)

| 헬퍼 | 설명 |
|---|---|
| `makeBadgeCell(text, objectName)` | QTableWidget 셀에 배지 위젯 삽입용 |
| `makeDivider()` | `QFrame#divider` 1px 구분선 생성 |
| `makeSearchBox(placeholder, width)` | 돋보기 아이콘 + QLineEdit 복합 위젯 |
| `BarChartWidget` | QPainter 기반 커스텀 바차트 (30개 데이터, 파란/주황 바, 빨간 점선 임계값) |
| `LineChartWidget` | QPainter 기반 커스텀 라인차트 (36개 데이터, 빨간 실선 + 주황 점선 임계값 + 교차 도트) |

---

### 대시보드 (`createDashboardPage`)

- 헤더에 "마지막 업데이트: 2026-04-23 14:22:33" 타임스탬프 추가
- **보호 상태 개요** 5개 행:
  `실시간 USB 모니터링` / `장치 제어 정책` / `비정상 입력 탐지` / `행위 분석` / `감사 로그`
  - 각 행 사이 구분선: `border-bottom: 1px solid #F1F5F9` 인라인 스타일
  - **마지막 행(감사 로그)에는 구분선 없음** (이전 버전의 버그 수정)

---

### USB 목록 (`createUsbListPage`) — Figma 기준 전면 재작성

**이전 → 이후**

| 항목 | 이전 | 이후 |
|---|---|---|
| 테이블 컬럼 수 | 7개 (동작 버튼 포함) | 6개 |
| 컬럼명 | 장치 이름/유형/VID-PID/연결 상태/정책/마지막 연결/동작 | 장치명/유형/VID-PID/상태/정책/마지막 확인 |
| 연결 상태 표시 | 배지 | 컬러 도트 + 텍스트 (`● 연결됨` / `● 해제됨`) |
| VID/PID 표시 | 일반 텍스트 | Consolas 폰트 + 회색 (`#62748E`) |
| 장치 수 | 5개 | 6개 (Razer DeathAdder 추가) |
| 오른쪽 패널 | 없음 | **장치 상세 정보** 340px 카드 |

**장치 상세 정보 패널 구성:**

```
[장치 아이콘 + 이름 + 제조사]

상태        ● 연결됨
적용 정책   허용 (green badge)
유형        입력 장치
VID / PID   046D : C52B  (code badge)
Serial No.  N/A
마지막 연결 방금 전

[이 장치 허용]   — 주황 버튼
[이 장치 차단]   — 분홍 버튼
[예외 규칙 삭제] — 흰 테두리 버튼
```

---

### 로그 (`createLogPage`)

- 헤더: 검색박스 + 기간 선택 / 이벤트 필터 버튼 + 내보내기(CSV) 주황 버튼
- 2패널 레이아웃: 로그 테이블 (flex:1) + 이벤트 상세 정보 (340px)
- 테이블 컬럼: 시간 / 이벤트 / 장치 / 처리 결과
- 배지 타입: `blockBadge`(자동 차단) / `successBadge`(허용) / `warningBadge`(경고 알림) / `neutralBadge`(업데이트·로그인)
- 오른쪽 패널: 이벤트 상세 정보 (장치명·설명·처리 결과·타임스탬프)

---

### 행위 분석 (`createAnalyticsPage`) — PNG 레퍼런스 기준 전면 재작성

**이전 → 이후**

| 항목 | 이전 | 이후 |
|---|---|---|
| 왼쪽 패널 | 바차트 + 미니 스탯 카드 3개 | 바차트 + **라인차트** (세로로 2개) |
| 오른쪽 패널 | 위험 점수 카드 + 최근 탐지 이력 카드 | **현재 세션 요약** 단일 카드 |

**라인차트 ("위험 점수 추이") 구성:**
- `LineChartWidget` — 누적 위험 점수 빨간 실선
- 임계값 8.0 주황 점선 + "임계값 8.0" 레이블
- 교차 지점 (~14:05) 주황 도트 마커
- Y축: 0.0 / 4.0 / 8.0 / 12.0, X축: 13:50 ~ 14:20

**현재 세션 요약 패널 구성:**

```
⚠ WARNING  불필요한 명령 시퀀스가 감지되었습니다.

세션 정보
  장치 유형    Keyboard (HID)
  시작 시각    2026-04-23 13:45:12
  지속 시간    00:37:21

입력 지표
  총 키 입력 수     1,234,567
  평균 입력 속도    3.2 keys/sec
  최대 입력 속도    11.6 keys/sec

위험 평가
  누적 위험 점수    9.5 / 12.0  (빨간색)
  설정된 임계값     8.0
```

---

### 설정 (`createSettingsPage`)

- 헤더: "변경 사항 저장" 주황 버튼
- 왼쪽: 탭 바 (일반/위협 탐지/알림) + `QStackedWidget` 로컬 탭 전환
- 오른쪽 340px: 적용된 정책 요약 카드 + 시스템 정보 카드

---

### 도움말 (`createHelpPage`) — 신규

- 헤더: 검색박스
- 2패널: 왼쪽 300px 주제 네비게이션 카드 + 오른쪽 콘텐츠 카드
- 주제: 시작하기 / 정책 관리 / 기능 가이드
- 콘텐츠: BadUSB 설명 heading2 + 단락 + heading3 + 불릿 리스트

---

## 2026-05-10 — UI 세부 수정 및 1차 백엔드 연결

- 로그 상세 패널의 `ACTION REQUIRED` 외곽 박스와 설명문 외곽 박스 제거
- USB 목록을 `HidScan::EnumeratePresentHidDevicePaths()` + `HidFingerprint::BuildUsbItemFromPath()` 결과로 구성
- USB 상세 패널의 `이 장치 허용` / `이 장치 차단` / `예외 규칙 삭제` 버튼을 `Policy` 저장소에 연결
- 정책 변경 시 테이블 배지와 오른쪽 상세 패널이 즉시 갱신되며 `Logger`에 정책 변경 로그 기록
- 로그 페이지는 정적 목 데이터 대신 `Logger::GetAll()`의 현재 로그를 기반으로 테이블 구성
