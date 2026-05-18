#pragma once
#include "framework.h"
#include "keyboard_hook.h"
#include <vector>

// TypingAnalyzer: 키입력 타이밍 분석 (인간 vs 자동화 판별)
// 전체 인터벌 기준 median + MAD 강건 통계로 단일 분류.
// 큰 대기 구간(다운로드 등)은 median/MAD 에 영향이 거의 없어 별도 처리 불필요.
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

        // 강건 통계
        double  medianMs;
        double  madMs;           // Median Absolute Deviation
        DWORD   minMs;
        DWORD   maxMs;

        // 참고
        double  meanMs;
        double  overallWpm;
    };

    Result Analyze(const std::vector<KeyboardHook::KeyRecord>& keys);
}
