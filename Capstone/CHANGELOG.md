# Abang-All — 작업 변경 기록

> 작성일: 2026-05-10  
> 대상 파일: `UIQt/MainWindow.cpp`, `Core/logger.cpp`

---

## 개요

BadUSB 탐지 프로그램(Abang-All)의 버그 수정 세션 기록.  
기능 추가는 없고 **오류 수정만** 수행.

---

## 수정 내역

### 1. 차단 장치 재연결 시 후킹 우회 버그

**파일:** `UIQt/MainWindow.cpp` — `nativeEvent` (DBT_DEVICEARRIVAL 처리)

**증상:**  
승인 대기 상태인 장치를 뺐다가 다시 꽂으면 입력 버퍼 모니터 다이얼로그가 ✕로 닫혔던 경우 후킹이 복구되지 않아 BadUSB 입력이 그대로 통과됨.

**원인:**  
`showBufferMonitorDialog`의 `destroyed` 핸들러가 다이얼로그 종료 시 무조건 `KeyboardHook::Uninstall()`을 호출했고, 재연결 시 이미 판정 대기 중인 장치에 대해 후킹을 재설치하지 않았음.

**수정:**  
`destroyed` 핸들러에서 종료 시 현재 연결된 HID 장치를 재열거하여 블랙리스트이거나 미승인 상태인 장치가 남아있으면 후킹을 유지하도록 변경.

```cpp
// destroyed 핸들러 (수정 후)
QObject::connect(dlg, &QObject::destroyed, [hookStartedByDialog, device] {
    if (!hookStartedByDialog) return;
    Policy::Load();
    bool keepHook = false;
    const auto devicePaths = HidScan::EnumeratePresentHidDevicePaths();
    for (const auto& path : devicePaths) {
        const UsbItem item = HidFingerprint::BuildUsbItemFromPath(path);
        if (item.fingerprint == 0) continue;
        if (Policy::IsBlacklisted(item.fingerprint)) { keepHook = true; break; }
        if (item.fingerprint == device.fingerprint
            && !Policy::IsWhitelisted(item.fingerprint))
            keepHook = true;
    }
    if (!keepHook) KeyboardHook::Uninstall();
});
```

---

### 2. 예외 규칙 삭제 시 연결 중인 장치 후킹 누락 버그

**파일:** `UIQt/MainWindow.cpp` — `removeRuleBtn` 클릭 핸들러

**증상:**  
허용/차단 정책이 등록된 장치가 연결된 상태에서 정책을 삭제해도 후킹이 즉시 시작되지 않아 그 사이 BadUSB 입력이 통과됨.

**원인:**  
`removeRuleBtn` 핸들러가 정책만 삭제하고 후킹 재설치 및 재검토 다이얼로그를 띄우지 않았음.

**수정:**  
정책 삭제 후 해당 장치가 현재 연결 중(`status == "연결됨"`)이면 즉시 후킹 설치 + 입력 버퍼 모니터 다이얼로그 재표시.  
재지문채취 없이 행에 저장된 `r.fingerprint`를 직접 사용 (장치가 이미 연결돼 있지 않을 수 있으므로).

```cpp
if (r.status == "연결됨" && r.fingerprint != 0) {
    if (!KeyboardHook::IsActive()) { KeyboardHook::ClearBuffer(); KeyboardHook::Install(); }
    lastPromptFingerprint_ = 0;
    UsbItem hookItem;
    hookItem.fingerprint = r.fingerprint;
    hookItem.name = toWString(r.name);
    hookItem.vid = r.vid; hookItem.pid = r.pid;
    hookItem.devicePath = r.devicePaths.empty() ? L"" : r.devicePaths.front();
    QTimer::singleShot(0, this, [this, hookItem] {
        showBufferMonitorDialog(nullptr, hookItem, [this] {
            enforceBlockedDeviceHook(); refreshLiveDataPages();
        });
    });
}
```

---

### 3. 허용/차단 버튼 — 연결 해제된 장치 재지문채취 실패

**파일:** `UIQt/MainWindow.cpp` — `allowDeviceBtn` / `blockDeviceBtn` 핸들러

**증상:**  
장치를 뽑은 후에 목록에서 허용/차단 버튼을 누르면 `BuildUsbItemFromPath()`가 경로를 찾지 못해 fingerprint = 0이 되어 정책이 저장되지 않음.

**수정:**  
행에 저장된 `r.fingerprint`, `r.vid`, `r.pid`를 직접 사용하여 `UsbItem`을 구성. 재지문채취 제거.

```cpp
UsbItem item;
item.fingerprint = r.fingerprint;
item.name = toWString(r.name);
item.vid = r.vid;
item.pid = r.pid;
item.allow = true;
Policy::AddWhitelist(item);
enforceBlockedDeviceHook();
```

---

### 4. 대시보드 숨김/최소화 상태에서 후킹 미작동

**파일:** `UIQt/MainWindow.cpp` — `showBufferMonitorDialog`

**증상:**  
메인 창이 트레이로 숨겨지거나 최소화된 상태에서 미등록 장치를 꽂으면 다이얼로그가 뜨지 않고 후킹도 작동하지 않음.

**원인:**  
`QDialog::exec()`이 부모 창에 종속된 중첩 이벤트 루프를 생성하여 메인 창이 숨김 상태일 때 블록됨. 또한 `exec()` 자체가 메인 스레드를 점유해 `WH_KEYBOARD_LL` 타임아웃을 유발.

**수정:**  
`exec()` → `show()` + `raise()` + `activateWindow()`로 교체. `parent = nullptr`로 독립 창 생성.

```cpp
// 수정 전
dlg->exec();

// 수정 후
dlg->show();
dlg->raise();
dlg->activateWindow();
```

---

### 5. 후킹 타이밍 너무 늦는 문제 (핵심 수정)

**파일:** `UIQt/MainWindow.cpp` — `nativeEvent` (DBT_DEVICEARRIVAL)

**증상:**  
장치 연결 후 CMD 창이 실행되고 명령어가 입력되는 중에 입력 버퍼 모니터 다이얼로그가 나타남. 즉, 후킹보다 BadUSB 실행이 먼저 완료되어 사실상 후킹이 무의미.

**원인:**  
`BuildUsbItemFromPath()`가 SetupAPI + HID 디스크립터 쿼리로 100~500ms 소요되는 무거운 작업인데 메인 스레드의 `QTimer::singleShot(0, ...)` 콜백 내에서 실행되어 메인 스레드가 블록됨.  
`WH_KEYBOARD_LL` 콜백은 설치 스레드(= 메인 스레드) 메시지 큐로 전달되는데, 메인 스레드가 `LowLevelHooksTimeout`(기본 300ms) 이상 응답하지 않으면 OS가 후킹을 **자동으로 우회**함.

**수정:**  
`BuildUsbItemFromPath()` + `Policy::Load()`를 `std::thread` 백그라운드로 이동.  
결과만 `QMetaObject::invokeMethod(..., Qt::QueuedConnection)`으로 메인 스레드에 전달.  
메인 스레드는 즉시 반환 → 후킹 콜백 처리 가능.

```cpp
// 수정 전 (메인 스레드 블록)
QTimer::singleShot(0, this, [this, path] {
    Policy::Load();
    const UsbItem item = HidFingerprint::BuildUsbItemFromPath(path);  // 블로킹!
    // ... 정책 판단 ...
});

// 수정 후 (백그라운드 스레드)
std::thread([this, path] {
    Policy::Load();
    const UsbItem item = HidFingerprint::BuildUsbItemFromPath(path);  // 백그라운드
    QMetaObject::invokeMethod(this, [this, item] {
        // ... 정책 판단 (메인 스레드에서 실행) ...
    }, Qt::QueuedConnection);
}).detach();
```

추가: `#include <thread>` 헤더 포함.

---

### 6. 로그 페이지 클릭 시 상세 정보 미갱신

**파일:** `UIQt/MainWindow.cpp` — `createLogPage()`, `appendNewLogRows()`

**증상:**  
로그 목록에서 다른 행을 클릭해도 오른쪽 이벤트 상세 정보 패널이 바뀌지 않음. 장치명도 항상 "N/A" 또는 "-" 표시.

**원인 1 — `Qt::NoItemFlags`:**  
메타데이터 저장용 `QTableWidgetItem`에 `setFlags(Qt::NoItemFlags)`를 설정하면 `Qt::ItemIsEnabled` 비트가 없어짐.  
`QAbstractItemView::mouseReleaseEvent` 내부에서 `isIndexEnabled()`가 false를 반환하면 `clicked` 시그널을 emit하지 않음 → `cellClicked`가 절대 발생하지 않음.

```cpp
// 수정 전
metaItem->setFlags(Qt::NoItemFlags);   // clicked 안 됨

// 수정 후
metaItem->setFlags(Qt::ItemIsEnabled); // clicked 정상 작동
```

적용 위치: `createLogPage()` (초기 행), `appendNewLogRows()` (신규 행) 두 곳.

**원인 2 — `WA_TransparentForMouseEvents` 미설정:**  
`setCellWidget()`으로 올린 위젯이 마우스 이벤트를 가로채 뷰포트까지 도달하지 않으면 `cellClicked`가 발생하지 않음.

```cpp
// 수정 후 — 셀 위젯 생성 시
w->setAttribute(Qt::WA_TransparentForMouseEvents);
lbl->setAttribute(Qt::WA_TransparentForMouseEvents);
```

**원인 3 — 장치명 "-" 표시:**  
로그 문자열에서 장치명을 추출하는 `extractDevice` / `extractDeviceAppend` 람다 추가.

```cpp
auto extractDevice = [](const QString& line) -> QString {
    static const QStringList kPrefixes = {
        "approval: ", "hook active: ", "hook state: ", "block: ", "allow: ",
        "Policy removed: ", "Policy allow: ", "Policy block: ", ...
    };
    for (const auto& p : kPrefixes) {
        const int idx = line.indexOf(p, 0, Qt::CaseInsensitive);
        if (idx != -1) {
            const QString candidate = line.mid(idx + p.length()).trimmed();
            if (!candidate.isEmpty()) return candidate;
        }
    }
    return "-";
};
```

---

### 7. 입력 버퍼 타임스탬프 뒷자리 숫자 변동

**파일:** `UIQt/MainWindow.cpp` — `formatBufferedKeysForView()`

**증상:**  
입력 버퍼 뷰에서 타임스탬프 끝 `.zzz` 부분(밀리초)이 250ms 갱신마다 매번 다른 값으로 바뀜.

**원인:**  
`KBDLLHOOKSTRUCT::time`은 `GetMessageTime()` 기반 32비트 틱으로 약 10~15ms 단위 정밀도.  
`QDateTime::currentDateTime()`은 1ms 정밀도.  
두 값의 해상도 차이로 인해 `addMSecs(k.time - nowTick)` 계산이 갱신마다 최대 15ms 오차 발생 → 밀리초 자리가 요동침.

**수정:**  
포맷 문자열에서 `.zzz` 제거, 초 단위까지만 표시.

```cpp
// 수정 전
.toString("yyyy-MM-dd HH:mm:ss.zzz")

// 수정 후
.toString("yyyy-MM-dd HH:mm:ss")
```

---

### 8. 입력 버퍼에 KEY_UP 이벤트 중복 표시

**파일:** `UIQt/MainWindow.cpp` — `formatBufferedKeysForView()`

**증상:**  
입력 버퍼 뷰에 KEY_DOWN, KEY_UP이 쌍으로 반복 출력되어 가독성 저하.

**원인:**  
`WH_KEYBOARD_LL`은 키 누름(WM_KEYDOWN)과 키 떼기(WM_KEYUP) 모두를 캡처함.  
버퍼에 두 이벤트가 모두 기록되어 뷰에도 그대로 표시됨.

**수정:**  
`LLKHF_UP` 플래그가 있는 이벤트는 뷰 출력에서 skip. 타이밍 분석도 원래부터 KEY_DOWN 간격만 사용하므로 동작 동일.

```cpp
const bool keyUp = (k.flags & LLKHF_UP) != 0;
if (keyUp) continue;  // KEY_UP 이벤트는 표시 생략
```

---

### 9. 로그 파일 영구 저장 추가

**파일:** `Core/logger.cpp`

**증상:**  
앱 재시작 시 로그가 전부 소실됨 (인메모리 벡터만 사용).

**수정:**  
`Logger::Add()` 호출 시 `%APPDATA%\AbangAll\abangall.log`에 UTF-8 + 타임스탬프로 추가 저장.

```
저장 경로: %APPDATA%\AbangAll\abangall.log
형식: 2026-05-10 14:32:59  [메시지]
인코딩: UTF-8 (BOM 없음)
```

---

### 10. X 버튼으로 닫아도 감지 계속 작동 안 하는 버그

**파일:** `UIQt/MainWindow.cpp`, `UIQt/MainWindow.h`

**증상:**  
메인 창을 X 버튼으로 닫으면 이후 BadUSB를 꽂아도 입력 버퍼 모니터가 뜨지 않음.

**원인:**  
`closeEvent` 오버라이드가 없어서 X 버튼이 창을 실제로 파괴함. `MainWindow`의 HWND가 사라지면 `WM_DEVICECHANGE` 메시지를 수신할 수 없어 USB 감지 자체가 중단됨. `app.setQuitOnLastWindowClosed(false)` 덕분에 프로세스는 살아있지만 감지가 작동하지 않음.

**수정:**  
`closeEvent`를 오버라이드하여 `event->ignore()` + `hide()`로 창을 트레이로 숨김. HWND가 유지되므로 `nativeEvent`가 계속 `WM_DEVICECHANGE`를 수신.

```cpp
void MainWindow::closeEvent(QCloseEvent* event)
{
    event->ignore();
    hide();
    if (trayIcon_)
        trayIcon_->showMessage("Abang-All", "백그라운드에서 계속 모니터링 중입니다.",
                               QSystemTrayIcon::Information, 2000);
}
```

---

### 11. 입력 버퍼 앞부분 짤림 — 팝업 즉시 표시

**파일:** `UIQt/MainWindow.cpp`

**증상:**  
BadUSB 꽂은 직후 타이핑된 앞부분 명령어가 잘려서 표시됨.  
예: `reg delete HKCU\...` → `rrmd1 /f` 처럼 초반 입력이 누락.

**원인:**  
기존 흐름: `DBT_DEVICEARRIVAL` → 훅 설치 → 400ms sleep → 지문채취(100~500ms) → 팝업  
팝업이 총 500~900ms 후에 열려, 그 사이 타이핑이 이미 많이 진행됨.  
훅은 즉시 설치되므로 버퍼에는 전부 잡혀있으나, 팝업이 늦게 열리면 사용자가 보는 시점이 늦음.

**수정:**  
`DBT_DEVICEARRIVAL` 도착 즉시 팝업 표시(장치 정보 없이 "장치 확인 중..."), 백그라운드 지문채취 후 장치 정보 업데이트.

- `g_activeBufferDlg` (파일 스코프 `QPointer<QDialog>`): 열린 다이얼로그 추적
- `g_dlgSharedDevice` (파일 스코프 `shared_ptr<UsbItem>`): 버튼 클릭 핸들러가 나중에 주입받는 장치 정보
- `addDevRow` 반환값: `dlg_device_name`, `dlg_vid_pid` objectName 레이블 → `findChild`로 업데이트
- sleep 400ms → 150ms 단축

```
도착 즉시: 훅 설치 + 팝업 오픈 ("장치 확인 중...")
           ↓
150ms 후:  지문채취 완료
           ├─ 화이트리스트 → 팝업 자동 닫기
           ├─ 블랙리스트  → 장치 정보 업데이트 (팝업 유지)
           └─ 미등록      → 장치 정보 업데이트 (팝업 유지, 허용/차단 가능)
```

---

## 미수정 / 설계 한계

| 항목 | 설명 |
|------|------|
| **장치별 선택적 차단** | `WH_KEYBOARD_LL`의 `KBDLLHOOKSTRUCT`에는 장치 식별자가 없음. 유저랜드 레벨에서는 특정 HID 장치만 선택적으로 차단 불가. 전체 키보드 입력을 막거나 허용하는 방식만 가능. 장치별 차단은 커널 드라이버(WDF 필터) 수준에서만 구현 가능. |
| **정책 저장** | 허용/차단 정책: `%APPDATA%\AbangAll\wb_db.json` (기존 원본 코드에서 이미 구현됨, 이번 세션에서 추가한 것 아님) |

---

## 수정된 파일 목록

| 파일 | 변경 내용 |
|------|----------|
| `UIQt/MainWindow.cpp` | 후킹 타이밍, 로그 클릭, 버퍼 뷰 표시, 정책 삭제/재연결, 허용/차단 버튼, X 버튼 트레이 숨김, 팝업 즉시 표시 |
| `UIQt/MainWindow.h` | `closeEvent` 선언 추가 |
| `Core/logger.cpp` | 로그 파일 영구 저장 |
