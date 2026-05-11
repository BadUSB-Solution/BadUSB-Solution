# UrlClassifier 구현 명세서

> 작성 기준: `key_decoder.h` (DecodeResult.lines[].urls)  
> URL 추출/디코딩은 PreAnalysis의 UrlExtractor가 이미 끝낸 상태.  
> 이 모듈은 **추출된 URL 문자열을 받아서 위험 신호를 분류**한다.

---

## 1. 위치

```
KeyDecoder (PreAnalysis)
    └─ UrlExtractor: 텍스트 → URL 문자열 추출/디코딩
              ↓
DecodeResult.lines[i].urls[j].resolved   ← UrlClassifier 입력
              ↓
UrlClassifier::Classify(decoded)         ← 이 모듈
              ↓
Result (URL별 분류 + 비트마스크)
              ↓
CompoundScorer (전 모듈 결과 종합)
```

---

## 2. 입력 데이터

`UrlExtractor` 가 채워둔 `UrlInfo`:

```cpp
struct UrlInfo {
    std::wstring raw;       // Chr() 포함 원문
    std::wstring resolved;  // 디코딩 완료된 URL  ← 분류 대상
    size_t       lineIndex;
};
```

**보장 사항:**
- `resolved` 는 `https://...` / `http://...` / `file://...` 등 정상 URL 포맷
- 퍼센트 디코딩, Chr() 재조합 모두 완료된 상태
- UrlClassifier는 URL 추출 로직 절대 중복 안 함

---

## 3. URL 컴포넌트 분해 (공통 전처리)

각 규칙 적용 전 URL 문자열을 부분별로 쪼갠다:

```cpp
struct ParsedUrl {
    std::wstring scheme;    // "https"
    std::wstring host;      // "bit.ly", "192.168.1.1"
    int          port;      // -1 (명시 안됨), 또는 명시된 포트
    std::wstring path;      // "/47OkUuD"
};

static ParsedUrl ParseUrl(const std::wstring& url);
```

**분해 흐름:**
```
"https://192.168.1.100:4444/payload.exe"
        ↓
scheme = "https"
host   = "192.168.1.100"
port   = 4444
path   = "/payload.exe"
```

수동 파싱 (`://`, `:`, `/` 분리). 정규식 가능하지만 단순 파싱이 명확.

---

## 4. 분류 규칙 (총 11종)

### 규칙 1: IP 주소 직접 접근

도메인 대신 IP 그대로 쓴 URL은 강한 의심 신호.  
세부 분류는 **사설/로컬호스트/링크로컬** 등으로 추가 분류.

```cpp
// inet_pton 으로 IPv4 파싱 후 IPHostFilter 비트 마스크 패턴 적용
struct in_addr addr;
std::string narrow = WideToNarrow(host);
if (inet_pton(AF_INET, narrow.c_str(), &addr) != 1)
    return false;

uint32_t ip = ntohl(addr.s_addr);
flags |= URL_FLAG_IP_ADDRESS;  // IP인 것 자체

if      ((ip & 0xFF000000) == 0x7F000000) flags |= URL_FLAG_IP_LOOPBACK;   // 127/8
else if ((ip & 0xFFFF0000) == 0xC0A80000 ||
         (ip & 0xFFF00000) == 0xAC100000 ||
         (ip & 0xFF000000) == 0x0A000000) flags |= URL_FLAG_IP_PRIVATE;    // 192.168/16, 172.16/12, 10/8
else if ((ip & 0xFFFF0000) == 0xA9FE0000) flags |= URL_FLAG_IP_LINKLOCAL;  // 169.254/16
```

→ `URL_FLAG_IP_ADDRESS` (+선택적으로 PRIVATE/LOOPBACK/LINKLOCAL)

---

### 규칙 2: 단축 URL

알려진 단축 서비스 도메인 매칭.  
**suffix 매칭** 사용 (서브도메인 케이스 안전 처리, 부분 문자열 오탐 방지).

```cpp
static constexpr const wchar_t* SHORTENER_DOMAINS[] = {
    L"bit.ly",     L"tinyurl.com", L"t.co",       L"goo.gl",
    L"is.gd",      L"buff.ly",     L"adf.ly",     L"ow.ly",
    L"shorturl.at",L"rb.gy",       L"cutt.ly",    L"rebrand.ly",
    L"tiny.cc",    L"url.kr",      L"vo.la",      L"han.gl",
    // 페이스트빈 계열 (페이로드 호스팅에 자주 사용)
    L"pastebin.com", L"hastebin.com", L"paste.ee",
};

bool MatchesSuffix(const std::wstring& host, const wchar_t* domain) {
    std::wstring d = domain;
    if (host == d) return true;
    if (host.size() > d.size() + 1 &&
        host.compare(host.size() - d.size() - 1, d.size() + 1, L"." + d) == 0)
        return true;
    return false;
}
```

→ `URL_FLAG_SHORTENER`

---

### 규칙 3: 실행파일 확장자

```cpp
static constexpr const wchar_t* DANGEROUS_EXTS[] = {
    L".exe", L".dll", L".scr",          // 네이티브 실행
    L".ps1", L".psm1",                  // PowerShell
    L".bat", L".cmd",                   // CMD
    L".hta", L".vbs", L".js", L".jse",  // 스크립트
    L".lnk",                            // 바로가기 (페이로드 트리거)
    L".msi",                            // 인스톨러
};
```

path 끝(쿼리스트링 제외)이 위 확장자 중 하나면 매칭.

→ `URL_FLAG_EXE_EXTENSION`

---

### 규칙 4: DGA / 고엔트로피 도메인

도메인 이름이 무작위 문자열(DGA — Domain Generation Algorithm) 같으면 C2 서버 의심.

**Shannon entropy** 계산:

```cpp
double ShannonEntropy(const std::wstring& s) {
    if (s.empty()) return 0.0;
    std::map<wchar_t, int> freq;
    for (auto c : s) freq[c]++;
    double H = 0.0;
    for (auto& [c, n] : freq) {
        double p = (double)n / s.size();
        H -= p * std::log2(p);
    }
    return H;
}
```

**판정 기준 (실험값):**
- 메인 도메인 라벨 길이 ≥ 8
- 엔트로피 > 3.5
- (보조) 자음 5자 이상 연속 시 가중

→ `URL_FLAG_HIGH_ENTROPY`

---

### 규칙 5: 비정상 스킴

```cpp
bool IsAbnormalScheme(const std::wstring& scheme) {
    auto s = ToLower(scheme);
    return s != L"http" && s != L"https";
}
```

비정상 예: `file://`, `ftp://`, `smb://`, `gopher://`, `javascript:`, `data:`

→ `URL_FLAG_ABNORMAL_SCHEME`

---

### 규칙 6: 비표준 포트

```cpp
bool IsUnusualPort(int port) {
    if (port < 0) return false;  // 명시 안됨 → 정상
    return port != 80 && port != 443 && port != 8080 && port != 8443;
}
```

C2 서버는 종종 `4444`, `8888`, `31337` 같은 비표준 포트 사용.

→ `URL_FLAG_UNUSUAL_PORT`

---

### 규칙 7: 무료 TLD (Freenom 계열)

거의 모든 사용이 악성. 매우 강한 신호.

```cpp
static constexpr const wchar_t* FREE_TLDS[] = {
    L".tk", L".ml", L".ga", L".cf", L".gq",
};

bool HasFreeTld(const std::wstring& host) {
    auto lower = ToLower(host);
    for (auto* tld : FREE_TLDS)
        if (EndsWith(lower, tld)) return true;
    return false;
}
```

→ `URL_FLAG_FREE_TLD`

---

### 규칙 8: 무료 정적 호스팅 플랫폼

개발자 사이트 정상 사용 많지만, BadUSB 페이로드 호스팅에도 자주 등장.  
**단독으론 약한 신호**, 다른 플래그(EXE_EXTENSION 등)와 조합 시 강해짐.

```cpp
static constexpr const wchar_t* FREE_HOSTING_DOMAINS[] = {
    L"github.io",       L"gitlab.io",
    L"netlify.app",     L"vercel.app",
    L"pages.dev",       L"web.app",
    L"firebaseapp.com", L"surge.sh",
    L"glitch.me",       L"repl.co",         L"replit.app",
};
```

suffix 매칭 사용.

→ `URL_FLAG_FREE_HOSTING`

---

### 규칙 9: 동적 DNS

C2 서버 흔적. 가정용 IP 주기적 갱신 도메인.

```cpp
static constexpr const wchar_t* DDNS_DOMAINS[] = {
    L"duckdns.org", L"ddns.net",   L"no-ip.com",
    L"hopto.org",   L"dyndns.org", L"sytes.net",
    L"zapto.org",
};
```

→ `URL_FLAG_DYNAMIC_DNS`

---

### 규칙 10: 파일 공유 / 직접 다운로드

```cpp
static constexpr const wchar_t* FILE_SHARING_DOMAINS[] = {
    L"mega.nz",      L"anonfiles.com", L"mediafire.com",
    L"transfer.sh",  L"file.io",       L"gofile.io",
};
```

→ `URL_FLAG_FILE_SHARING`

---

### 규칙 11: 터널링 서비스 (임시 노출)

ngrok 등은 보통 개발/테스트용이지만, 공격자가 임시 C2 / 페이로드 호스팅에 사용.

```cpp
static constexpr const wchar_t* TUNNELING_DOMAINS[] = {
    L"ngrok.io",          L"ngrok.app",
    L"serveo.net",        L"localtunnel.me",
    L"loca.lt",           L"trycloudflare.com",
};
```

→ `URL_FLAG_TUNNELING`

---

## 5. 출력 구조

```cpp
namespace UrlClassifier
{
    enum UrlFlag : uint32_t {
        URL_FLAG_NONE             = 0,

        // 규칙 1: IP 주소 (세분화)
        URL_FLAG_IP_ADDRESS       = 1 << 0,
        URL_FLAG_IP_PRIVATE       = 1 << 1,
        URL_FLAG_IP_LOOPBACK      = 1 << 2,
        URL_FLAG_IP_LINKLOCAL     = 1 << 3,

        // 규칙 2: 단축 URL
        URL_FLAG_SHORTENER        = 1 << 4,

        // 규칙 3: 실행파일 확장자
        URL_FLAG_EXE_EXTENSION    = 1 << 5,

        // 규칙 4: 고엔트로피 도메인
        URL_FLAG_HIGH_ENTROPY     = 1 << 6,

        // 규칙 5: 비정상 스킴
        URL_FLAG_ABNORMAL_SCHEME  = 1 << 7,

        // 규칙 6: 비표준 포트
        URL_FLAG_UNUSUAL_PORT     = 1 << 8,

        // 규칙 7~11: 무료/위장 호스팅 (세분화)
        URL_FLAG_FREE_TLD         = 1 << 9,
        URL_FLAG_FREE_HOSTING     = 1 << 10,
        URL_FLAG_DYNAMIC_DNS      = 1 << 11,
        URL_FLAG_FILE_SHARING     = 1 << 12,
        URL_FLAG_TUNNELING        = 1 << 13,
    };

    struct UrlClassification {
        std::wstring url;        // resolved URL 문자열
        size_t       lineIndex;
        uint32_t     flags;      // UrlFlag 비트마스크
    };

    struct Result {
        std::vector<UrlClassification>  items;
        uint32_t                         aggregateFlags;  // 전체 OR
    };

    Result Classify(const KeyDecoder::DecodeResult& decoded);
}
```

---

## 6. API 시그니처

```cpp
namespace UrlClassifier
{
    Result Classify(const KeyDecoder::DecodeResult& decoded);
}
```

내부적으로:
```
for each line in decoded.lines:
    for each url in line.urls:
        ParsedUrl p = ParseUrl(url.resolved);
        flags = 0;
        // 규칙 1~11 적용
        items.push_back({ url.resolved, url.lineIndex, flags });
        aggregateFlags |= flags;
```

---

## 7. 작업 순서

| # | 작업 | 비고 |
|---|------|------|
| 1 | 헤더 인터페이스 정의 (UrlFlag enum, 구조체, API) | `url_classifier.h` |
| 2 | `ParseUrl` 컴포넌트 분해 헬퍼 | `url_classifier.cpp` |
| 3 | 헬퍼: `ToLower`, `EndsWith`, `MatchesSuffix`, `WideToNarrow` | 〃 |
| 4 | 규칙 1: IP (inet_pton + 비트마스크) | `ws2_32.lib` 링크 필요 |
| 5 | 규칙 2: 단축 URL (suffix 매칭) | |
| 6 | 규칙 3: 실행파일 확장자 | |
| 7 | 규칙 4: Shannon entropy | |
| 8 | 규칙 5: 비정상 스킴 | |
| 9 | 규칙 6: 비표준 포트 | |
| 10 | 규칙 7~11: 무료 호스팅 도메인 5종 | |
| 11 | `Classify()` 통합 | |
| 12 | 디버그 창에 분류 결과 표시 | `ui_buffer_view.cpp` |
| 13 | 빌드 검증 | |

---

## 8. 디버그 창 표시 (제안)

`OpenNormalizeDebug` 의 URLs 섹션 확장:

```
=== URLs ===
[line 0] https://bit.ly/47OkUuD
         flags: SHORTENER
[line 1] https://attacker.github.io/payload.exe
         flags: FREE_HOSTING | EXE_EXTENSION
[line 2] http://192.168.1.100:4444/x.ps1
         flags: IP_ADDRESS | IP_PRIVATE | EXE_EXTENSION | UNUSUAL_PORT
[line 3] https://kj3xq8a9z.tk/c2
         flags: FREE_TLD | HIGH_ENTROPY
```

---

## 9. 테스트 케이스

| URL | 예상 flags |
|-----|----------|
| `https://google.com/search?q=test` | `0` (정상) |
| `https://github.com/user/repo` | `0` (github.com은 정상, github.io만 무료 호스팅) |
| `https://bit.ly/47OkUuD` | `SHORTENER` |
| `https://attacker.github.io/payload.exe` | `FREE_HOSTING \| EXE_EXTENSION` |
| `http://192.168.1.100:4444/x.ps1` | `IP_ADDRESS \| IP_PRIVATE \| EXE_EXTENSION \| UNUSUAL_PORT` |
| `https://127.0.0.1/test` | `IP_ADDRESS \| IP_LOOPBACK` |
| `file:///C:/payload.exe` | `ABNORMAL_SCHEME \| EXE_EXTENSION` |
| `https://kj3xq8a9zpqm.tk/x` | `FREE_TLD \| HIGH_ENTROPY` |
| `https://atk.duckdns.org:8888/c2` | `DYNAMIC_DNS \| UNUSUAL_PORT` |
| `https://abc.ngrok.io/payload.hta` | `TUNNELING \| EXE_EXTENSION` |
| `https://mega.nz/file/abc123` | `FILE_SHARING` |

---

## 10. DB 활용 여부

- 단축/무료 호스팅/DDNS 도메인 리스트는 **수십 개 수준** → static array 충분
- DB 쿼리 오버헤드 불필요
- 향후 외부 평판 데이터(VirusTotal, PhishTank) 연동 시 그때 DB/네트워크 검토

---

## 11. 알려진 한계

| 항목 | 설명 |
|------|------|
| **고엔트로피 임계값** | 3.5는 실험값. 정상 도메인(`stackoverflow.com`)도 일부 걸릴 수 있음 → 길이/자모음 비율 보조 검사 권장 |
| **eTLD 미처리 (PSL 없음)** | `bit.ly.evil.com` 같은 위장 도메인 검사 불완전. suffix 매칭으로 대부분 커버되지만 완벽하지 않음 |
| **외부 평판 DB 없음** | VirusTotal, PhishTank, URLhaus 같은 실제 악성 도메인 DB 미연동. 알려진 도메인만 매칭 |
| **국제화 도메인(IDN)** | 푸니코드(`xn--...`) 변환 처리 없음. `аpple.com` (키릴문자) 같은 호모그래프 공격 미감지 |
| **무료 호스팅 단독 신호 약함** | `github.io`, `netlify.app` 는 정상 사용 압도적. 다른 플래그와 조합 시에만 의미. CompoundScorer에서 가중치 처리 필요 |
| **IPv6 미지원** | `inet_pton(AF_INET6, ...)` 추가 필요. 현재 IPv4만 처리 |
| **PSL 미사용** | `co.kr`, `github.io` 같은 eTLD 정확 분리 불가. 도메인 비교가 단순 suffix 기반 |

---

## 12. CompoundScorer 와의 관계

UrlClassifier는 **사실 분류만** 한다. 점수 산정 안 함.

CompoundScorer 에서 다음과 같이 활용:

```
ShellDetector(SHELL_CMD/PS) +
ThreatMatcher(DOWNLOAD/EXECUTOR 명령어) +
UrlClassifier(SHORTENER + EXE_EXTENSION + FREE_TLD)
        ↓
"BadUSB 공격 패턴" 종합 판정
```

특히 강한 조합:
- `Win+R` 핫키 + DOWNLOAD 명령어 + `SHORTENER` + `EXE_EXTENSION`  → BADUSB_CLASSIC
- `mshta` (EXECUTOR) + `FREE_TLD`/`DYNAMIC_DNS`                       → DIRECT_REMOTE_EXEC
- DOWNLOAD + `IP_ADDRESS` + `UNUSUAL_PORT`                            → C2_DOWNLOAD
- `iwr` -OutFile `%TEMP%\hack.exe` + `FREE_HOSTING`                   → 무료 호스팅 페이로드

CompoundScorer 영역은 이 명세 밖.

---

## 13. 구현 우선순위

1. **헬퍼 + 규칙 1~3** (IP, 단축 URL, 실행파일 확장자) — 가장 직관적, 효과 큼
2. **규칙 5, 6** (비정상 스킴, 비표준 포트) — 단순 비교
3. **규칙 7~11** (무료 호스팅 5종) — 정적 리스트 매칭만
4. **규칙 4** (Shannon entropy) — 임계값 튜닝 필요, 마지막
5. **디버그 창 표시 + 빌드 검증**
