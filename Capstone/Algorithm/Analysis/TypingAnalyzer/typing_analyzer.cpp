#include "typing_analyzer.h"
#include <algorithm>
#include <cmath>

namespace TypingAnalyzer
{
    // ── 임계값 (실험값) ──
    static constexpr size_t MIN_SAMPLES        = 5;     // 최소 인터벌 수
    static constexpr double FAST_THRESHOLD_MS  = 30.0;  // 중앙값 빠름
    static constexpr double UNIFORM_MAD_MS     = 5.0;   // MAD 균일

    static constexpr DWORD  KEYUP_FLAG = 0x80;  // LLKHF_UP

    // ── 헬퍼 ──

    static bool IsModifierVk(DWORD vk)
    {
        return vk == VK_LSHIFT   || vk == VK_RSHIFT   || vk == VK_SHIFT   ||
               vk == VK_LCONTROL || vk == VK_RCONTROL || vk == VK_CONTROL ||
               vk == VK_LMENU    || vk == VK_RMENU    || vk == VK_MENU    ||
               vk == VK_LWIN     || vk == VK_RWIN     || vk == VK_CAPITAL;
    }

    static double MedianSorted(const std::vector<double>& v)
    {
        size_t n = v.size();
        if (n == 0) return 0.0;
        if (n % 2 == 0) return (v[n/2 - 1] + v[n/2]) / 2.0;
        return v[n/2];
    }

    static double Median(const std::vector<DWORD>& intervals)
    {
        if (intervals.empty()) return 0.0;
        std::vector<double> v(intervals.begin(), intervals.end());
        std::sort(v.begin(), v.end());
        return MedianSorted(v);
    }

    static double MedianAbsoluteDeviation(const std::vector<DWORD>& intervals,
                                          double median)
    {
        if (intervals.empty()) return 0.0;
        std::vector<double> absDevs;
        absDevs.reserve(intervals.size());
        for (auto x : intervals)
            absDevs.push_back(std::abs(static_cast<double>(x) - median));
        std::sort(absDevs.begin(), absDevs.end());
        return MedianSorted(absDevs);
    }

    // ── 메인 ──

    Result Analyze(const std::vector<KeyboardHook::KeyRecord>& keys)
    {
        Result r{};
        r.classification = TYPING_UNKNOWN;

        // 1) keydown만, modifier 제외해서 timestamp 모음
        std::vector<DWORD> times;
        times.reserve(keys.size());
        for (const auto& k : keys) {
            if (k.flags & KEYUP_FLAG) continue;
            if (IsModifierVk(k.vkCode)) continue;
            times.push_back(k.time);
        }
        r.keyCount = times.size();
        if (times.size() < 2) return r;

        // 2) 인터벌 계산
        std::vector<DWORD> intervals;
        intervals.reserve(times.size() - 1);
        for (size_t i = 1; i < times.size(); ++i)
            intervals.push_back(times[i] - times[i-1]);

        r.intervalCount = intervals.size();
        r.minMs = *std::min_element(intervals.begin(), intervals.end());
        r.maxMs = *std::max_element(intervals.begin(), intervals.end());

        // 3) 통계
        double sum = 0.0;
        for (auto x : intervals) sum += x;
        r.meanMs   = sum / intervals.size();
        r.medianMs = Median(intervals);
        r.madMs    = MedianAbsoluteDeviation(intervals, r.medianMs);

        // 4) WPM (참고용, 대기 포함)
        DWORD totalMs = times.back() - times.front();
        if (totalMs > 0) {
            double minutes = totalMs / 60000.0;
            r.overallWpm = (r.keyCount / 5.0) / minutes;
        }

        // 5) 분류
        //   - 중앙값이 매우 짧고 (FAST_THRESHOLD 미만)
        //   - 변동성도 매우 낮으면 (MAD UNIFORM 미만)
        //   → AUTOMATED
        //   median/MAD 둘 다 outlier(다운로드 대기 등)에 강건해서 별도 필터 불필요
        if (r.intervalCount < MIN_SAMPLES) {
            r.classification = TYPING_UNKNOWN;
        } else {
            bool veryFast    = r.medianMs < FAST_THRESHOLD_MS;
            bool veryUniform = r.madMs    < UNIFORM_MAD_MS;
            r.classification = (veryFast && veryUniform) ? TYPING_AUTOMATED
                                                          : TYPING_HUMAN;
        }

        // 6) 점수
        r.score = ComputeScore(r);

        return r;
    }

    // ──────────────────────────────────────────────────────────────────
    // 점수 정책
    //   - HUMAN / UNKNOWN: 0
    //   - AUTOMATED 베이스: 5
    //     median 이 임계값보다 훨씬 짧으면 가산:
    //       < 10ms → +3 (매우 명백한 자동화)
    //       < 20ms → +1.5
    //     MAD 가 임계값보다 훨씬 작으면 가산:
    //       <  1ms → +2 (거의 완벽한 균일성)
    //       <  3ms → +1
    //
    //   → 최대 10점 (매우 빠르고 매우 균일한 BadUSB 입력)
    //   → 최소 5점 (AUTOMATED 분류만 받은 상태)
    // ──────────────────────────────────────────────────────────────────
    static constexpr double SCORE_CAP = 10.0;

    double ComputeScore(const Result& r)
    {
        if (r.classification != TYPING_AUTOMATED) return 0.0;

        double s = 5.0;  // AUTOMATED 베이스

        // median 추가 점수 (더 빠를수록 강한 시그널)
        if (r.medianMs < 10.0)      s += 3.0;
        else if (r.medianMs < 20.0) s += 1.5;

        // MAD 추가 점수 (더 균일할수록 강한 시그널)
        if (r.madMs < 1.0)      s += 2.0;
        else if (r.madMs < 3.0) s += 1.0;

        return (std::min)(SCORE_CAP, s);
    }
}
