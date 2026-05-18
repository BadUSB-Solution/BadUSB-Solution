# TypingAnalyzer 구현 명세서

> 키입력 타이밍을 분석해 **인간 타이핑 vs 자동화 입력**(BadUSB)을 판별.  
> 전체 인터벌 기준 **median + MAD** 강건 통계로 단일 분류.  
> 큰 대기 구간(다운로드 등)은 median/MAD 에 영향 거의 없어 별도 처리 불필요.

---

## 1. 위치

```
KeyboardHook → KeyRecord[] (vkCode, scanCode, flags, time)
        ↓
TypingAnalyzer::Analyze(keys)  ← 이 문서 대상
        ↓
TypingResult (classification + 통계)
        ↓
CompoundScorer (전 모듈 종합)
```

PreAnalysis 폴더에 있지만 텍스트 변환은 안 함. **순수 타이밍 분석.**

---

## 2. 입력 데이터

```cpp
struct KeyboardHook::KeyRecord {
    DWORD vkCode;
    DWORD scanCode;
    DWORD flags;
    DWORD time;     // 시스템 tick (ms)
};
```

`time` 필드는 KeyboardHook 이 이미 기록 중. 별도 작업 불필요.

---

## 3. 설계 핵심

### 3.1 강건 통계 사용 이유

BadUSB 공격에서 자주 보이는 패턴:
```
[빠른 50회 타이핑(~5ms)] → Enter → [다운로드 대기 5초] → [빠른 30회] → Enter
```

평균/표준편차로 보면 5초 대기 때문에 통계가 왜곡됨.  
**median + MAD**는 이상치 영향이 거의 없어서 단일 통계로 안전:

```
intervals = [5, 5, 5, ..., 5, 5000]  (49개 + outlier 1개)

평균:    105ms        ← 왜곡됨
표준편차: ~700ms       ← 왜곡됨
median:  5ms          ← 강건 ✓
MAD:     0ms          ← 강건 ✓
```

→ 버스트 분할/이상치 필터링 불필요. 단일 통계만 계산.

### 3.2 max는 분류에서 빼는 이유

`maxMs` 는 통계로 출력은 하지만 **분류 기준에는 사용 안 함.**  
하나의 대기 구간만 있어도 max 가 큰 값이 되어 잘못된 판단 가능.  
median + MAD 만으로 충분.

---

## 4. 출력 구조

```cpp
namespace TypingAnalyzer
{
    enum TypingClass : int {
        TYPING_UNKNOWN   = 0,   // 데이터 부족
        TYPING_HUMAN     = 1,   // 인간 타이핑
        TYPING_AUTOMATED = 2,   // 자동화 의심
    };

    struct Result {
        TypingClass classification;

        size_t  keyCount;        // 필터 후 키 수
        size_t  intervalCount;   // 인터벌 수 (= keyCount - 1)

        // 강건 통계 (분류 기준)
        double  medianMs;
        double  madMs;           // Median Absolute Deviation

        // 참고 통계 (분류엔 사용 안 함)
        DWORD   minMs;
        DWORD   maxMs;
        double  meanMs;
        double  overallWpm;

        // 0~10 위협 점수 (CompoundScorer 입력)
        double  score;
    };

    Result Analyze(const std::vector<KeyboardHook::KeyRecord>& keys);
    double ComputeScore(const Result& r);   // 정책 단일 출처 (외부 노출)
}
```

---

## 5. API 시그니처

```cpp
namespace TypingAnalyzer
{
    // 메인 진입점
    Result Analyze(const std::vector<KeyboardHook::KeyRecord>& keys);

    // Result → 0~10 점수 변환 (정책 단일 출처). 외부에서 임의 평가 가능.
    double ComputeScore(const Result& r);
}
```

---

## 6. 처리 알고리즘

### 6.1 키 필터링

분석 대상만 추림:
- **keyup 제외** (`flags & 0x80`)
- **modifier 키 제외** (Shift/Ctrl/Alt/Win/CapsLock)
  - Shift+A 같은 케이스에서 modifier-down → key-down 간격이 매우 짧아 평균 왜곡

```cpp
static bool IsModifierVk(DWORD vk)
{
    return vk == VK_LSHIFT   || vk == VK_RSHIFT   || vk == VK_SHIFT   ||
           vk == VK_LCONTROL || vk == VK_RCONTROL || vk == VK_CONTROL ||
           vk == VK_LMENU    || vk == VK_RMENU    || vk == VK_MENU    ||
           vk == VK_LWIN     || vk == VK_RWIN     || vk == VK_CAPITAL;
}
```

### 6.2 인터벌 계산

```cpp
std::vector<DWORD> times;
for (auto& k : keys) {
    if (k.flags & 0x80) continue;
    if (IsModifierVk(k.vkCode)) continue;
    times.push_back(k.time);
}

std::vector<DWORD> intervals;
for (size_t i = 1; i < times.size(); ++i)
    intervals.push_back(times[i] - times[i-1]);
```

### 6.3 통계 계산

```cpp
double Median(const std::vector<DWORD>& v) {
    auto sorted = ...sort copy...;
    size_t n = sorted.size();
    if (n == 0) return 0.0;
    return (n % 2 == 0) ? (sorted[n/2 - 1] + sorted[n/2]) / 2.0
                        : (double)sorted[n/2];
}

double MAD(const std::vector<DWORD>& v, double median) {
    std::vector<double> absDevs;
    for (auto x : v) absDevs.push_back(std::abs((double)x - median));
    std::sort(absDevs.begin(), absDevs.end());
    return MedianSorted(absDevs);
}
```

min/max/mean은 보조용. 분류엔 사용 안 함.

### 6.4 분류

```cpp
static constexpr size_t MIN_SAMPLES        = 5;
static constexpr double FAST_THRESHOLD_MS  = 30.0;
static constexpr double UNIFORM_MAD_MS     = 5.0;

if (intervalCount < MIN_SAMPLES)
    return TYPING_UNKNOWN;

bool veryFast    = medianMs < FAST_THRESHOLD_MS;  // 중앙값 < 30ms
bool veryUniform = madMs    < UNIFORM_MAD_MS;     // MAD < 5ms

return (veryFast && veryUniform) ? TYPING_AUTOMATED : TYPING_HUMAN;
```

### 6.5 WPM (참고용)

```cpp
// 분당 단어 수 (5자 = 1단어 관습), 대기 시간 포함
DWORD totalMs = times.back() - times.front();
double minutes = totalMs / 60000.0;
overallWpm = (keyCount / 5.0) / minutes;
```

대기 포함되면 실제 타이핑 속도보다 낮게 나오지만 디버그 표시용.

### 6.6 점수 (Score)

분류 결과와 통계를 바탕으로 0~10 점수 산출. CompoundScorer 입력으로 사용.

```cpp
r.score = ComputeScore(r);
```

자세한 정책은 §7 참고.

---

## 7. 점수 산정 (Scoring)

### 7.1 정책 개요

```
HUMAN / UNKNOWN: 0
AUTOMATED 베이스: 5
  median 추가 점수 (더 빠를수록 강한 시그널):
    < 10ms  →  +3.0
    < 20ms  →  +1.5
  MAD 추가 점수 (더 균일할수록 강한 시그널):
    <  1ms  →  +2.0
    <  3ms  →  +1.0
최종 점수: min(10, base + median 보정 + MAD 보정)
```

**결과 범위:**
- `TYPING_HUMAN` / `TYPING_UNKNOWN` → 0점
- `TYPING_AUTOMATED` 만 받은 경우 → 최소 **5점** (베이스만)
- 매우 빠르고 매우 균일한 BadUSB → **10점 만점**

### 7.2 점수 예시

| 시나리오 | classification | median | MAD | 점수 |
|---------|--------------|--------|-----|------|
| 사람 정상 타이핑 | HUMAN | 120 | 50 | **0** |
| 데이터 부족 | UNKNOWN | — | — | **0** |
| AUTOMATED 임계값 근처 | AUTOMATED | 25 | 4 | 5.0 |
| 빠른 AUTOMATED | AUTOMATED | 15 | 4 | 5.0 + 1.5 = **6.5** |
| 매우 빠르고 매우 균일 (BadUSB 전형) | AUTOMATED | 5 | 0 | 5.0 + 3.0 + 2.0 = **10.0** |
| 빠르지만 약간 변동 | AUTOMATED | 8 | 2 | 5.0 + 3.0 + 1.0 = **9.0** |

### 7.3 CompoundScorer 연계

```cpp
auto ta = TypingAnalyzer::Analyze(keys);
auto v  = CompoundScorer::Combine(tm_score, url_score, ta.score, hk_score);
```

CompoundScorer 가중치:
```
W_TM     = 1.0
W_URL    = 0.2
W_TYPING = 0.3   ← TypingAnalyzer 기여도
W_HOTKEY = 0.5
```

→ TypingAnalyzer `score=10` 일 때 종합에 +3.0 기여.

---

## 8. 임계값 정리

| 상수 | 값 | 의미 |
|------|----|----|
| `MIN_SAMPLES` | 5 | 분류 위한 최소 인터벌 수 |
| `FAST_THRESHOLD_MS` | 30 | 중앙값이 이보다 작으면 "빠름" |
| `UNIFORM_MAD_MS` | 5 | MAD가 이보다 작으면 "균일" |
| (점수 가산) median 추가 | 10 / 20 | +3.0 / +1.5 |
| (점수 가산) MAD 추가 | 1 / 3 | +2.0 / +1.0 |

모두 실험값. BadUSB 샘플로 조정 필요.

---

## 9. 디버그 창 표시

```
=== Typing Analysis ===
Classification: AUTOMATED   score=10.00 / 10   keys=83  WPM=156
  median=7ms  MAD=1ms  mean=152ms  range=3..5000ms
```

해석:
- `score=10.00` → AUTOMATED 베이스(5) + median<10(+3) + MAD<1(+2) = 10
- `median=7ms` → 매우 빠름 (FAST_THRESHOLD 30 미만)
- `MAD=1ms` → 매우 균일 (UNIFORM_MAD 5 미만)
- `max=5000ms` → 대기 구간 있음. **median/MAD 는 영향 없음.**
- `mean=152ms` 와 `median=7ms` 의 큰 차이가 "대기 끼어있는 균일 자동 입력" 시그너처

---

## 10. 처리 흐름 요약

```
KeyRecord[]
    ↓ 6.1 필터 (keyup/modifier 제외)
times[]
    ↓ 6.2 인터벌 계산
intervals[]
    ↓ 6.3 통계 (median, MAD, min, max, mean)
    ↓ 6.4 분류 (median + MAD 기준)
Result
```

---

## 11. 파일 구조

```
Capstone/Algorithm/PreAnalysis/TypingAnalyzer/
├── TYPING_ANALYZER_SPEC.md    (이 문서)
├── typing_analyzer.h          (인터페이스)
└── typing_analyzer.cpp        (구현)
```

vcxproj 등록 완료. 추가 작업 없음.

---

## 12. 테스트 시나리오

| 시나리오 | 입력 | 예상 결과 |
|---------|------|---------|
| **정상 타이핑** | 사람이 50자 입력 (간격 80~200ms) | TYPING_HUMAN, median ≈ 120, MAD ≈ 50 |
| **빠른 정상 타이핑** | 숙련자, 간격 60~120ms | TYPING_HUMAN, median ≈ 80, MAD ≈ 20 (UNIFORM_MAD 초과) |
| **BadUSB 단순** | 균일한 5ms 간격 50회 | TYPING_AUTOMATED, median ≈ 5, MAD ≈ 0~2 |
| **BadUSB + 대기** | 50회(5ms) + 5초대기 + 30회(5ms) | TYPING_AUTOMATED (median/MAD 안 흔들림) |
| **사람 → BadUSB** | 사람 10키 + 대기 + BadUSB 40키 | 다수 BadUSB 인터벌 → median 짧음 → AUTOMATED |
| **짧은 입력** | 3키만 입력 | TYPING_UNKNOWN |

---

## 13. 알려진 한계

| 항목 | 설명 |
|------|------|
| **DWORD time 해상도** | `GetTickCount()` 단위 ~15ms. 매우 빠른 입력 미세 구분 어려움 |
| **Tick wraparound** | 49.7일 후 wrap. BadUSB 분석 컨텍스트에선 무시 가능 |
| **사람 빠른 타이핑 오탐** | 분당 200타 이상 숙련자 BadUSB로 오탐 가능 → 임계값 튜닝 필요 |
| **혼합 입력 한계** | 사람 + BadUSB 비율에 따라 결과 갈림. median 우세 측이 분류 결정 |
| **modifier 영향 잔존** | Shift/Ctrl 제외해도 단축키 콤보 영향은 남음 |
| **시작 지연 미반영** | 창 활성화 후 첫 키까지 지연(사람 ≥200ms vs BadUSB 거의 없음) 미고려 |

---

## 14. CompoundScorer 와의 관계

TypingAnalyzer 는 분류 + **0~10 점수** 까지 산출. CompoundScorer 에서 가중치 합산.

```cpp
auto ta = TypingAnalyzer::Analyze(keys);
auto v  = CompoundScorer::Combine(tm_score, url_score, ta.score, hk_score);
```

CompoundScorer 가중치 (`compound_scorer.cpp`):
```
W_TM     = 1.0
W_URL    = 0.2
W_TYPING = 0.3   ← TypingAnalyzer 기여도 (활성화됨)
W_HOTKEY = 0.5
```

→ TypingAnalyzer `score=10` 일 때 종합에 +3.0 기여.

### 패턴 조합 (참고)

- `TYPING_AUTOMATED` (score≥8) + 다른 모듈 시그널 (Win+R + DOWNLOAD + 단축 URL 등) → BadUSB 강한 증거
- `TYPING_HUMAN` (score=0) + 위협 명령어 → 사용자 의도 가능성 (false positive 감소 단서)
- `TYPING_UNKNOWN` (score=0) → 다른 모듈 결과에 가중치 양보

---

## 15. 향후 보강 옵션 (선택)

기본 구현 후 정확도 더 올리고 싶을 때:

| 보강 | 효과 | 복잡도 |
|------|------|------|
| 첫 입력 지연(activation latency) 검사 | BadUSB 시작 지연 거의 0 | 낮음 |
| Backspace/Arrow 비율 | 사람은 오타 수정 있음 | 낮음 |
| N-gram 분석 | 같은 시퀀스가 너무 일정하면 의심 | 중간 |
| 머신러닝 모델 | 정확도 ↑↑ | 매우 높음 |

1차 구현은 휴리스틱만. 추후 필요시 추가.

---

## 16. 설계 변경 이력

| 버전 | 변경 | 이유 |
|------|------|------|
| 초안 | 버스트 분할 (gap > 500ms 기준) | 대기 구간 대응 |
| v2 | 버스트 분할 제거, 단일 통계 | median + MAD가 이미 outlier에 강건해서 분할 불필요. 코드 단순화 |
| **현재** | `score` 필드 + `ComputeScore()` 추가, CompoundScorer 가중치 0.0→0.3 활성화 | 0~10 점수 산출로 CompoundScorer 입력 표준화 |
