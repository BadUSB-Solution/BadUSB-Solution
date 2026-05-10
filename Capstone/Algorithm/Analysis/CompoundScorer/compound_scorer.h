#pragma once
#include "framework.h"
#include <string>

// ──────────────────────────────────────────────────────────────────────
// CompoundScorer
//   각 분석 모듈의 점수를 가중 평균해 최종 위협 점수와 4단계 분류 산출.
//
// 호출 패턴 (Pattern B — caller 가 모듈 호출 후 결과 전달):
//   auto shell = ShellDetector::Detect(decoded);
//   auto tm    = ThreatMatcher::Analyze(decoded, shell.shell);
//   auto hk    = HotkeyScorer::Analyze(decoded.hotkeys);
//   // double url    = UrlClassifier::Score(decoded);    // 미구현
//   // double typing = TypingAnalyzer::Score(keys);      // 미구현
//   auto verdict = CompoundScorer::Combine(tm.session_score, 0, 0, hk.final_score);
//
// 점수 정책: 점수체계_통합본.md 참조.
// ──────────────────────────────────────────────────────────────────────
namespace CompoundScorer
{
    // 4단계 위협 등급
    //   임계값 (점수체계_통합본.md, 임시값 — 운영 calibration 시 조정):
    //     SAFE   : final_score <  3.0
    //     NORMAL : final_score <  5.0
    //     WARN   : final_score <  7.5
    //     DANGER : final_score >= 7.5
    enum Level : int {
        SAFE   = 0,
        NORMAL = 1,
        WARN   = 2,
        DANGER = 3
    };

    // 최종 판정 결과
    struct Verdict {
        double  final_score;     // 0~10 가중 평균 (cap)
        Level   level;           // 4단계 분류

        // 모듈별 입력 점수 (디버깅/UI 표시용 보관)
        double  tm_score;        // ThreatMatcher session_score
        double  url_score;       // UrlClassifier (미구현 시 0)
        double  typing_score;    // TypingAnalyzer (미구현 시 0)
        double  hotkey_score;    // HotkeyScorer (Win+R, Ctrl+Shift+Enter 등)

        // 사용된 가중치 (calibration 추적용)
        double  tm_weight;
        double  url_weight;
        double  typing_weight;
        double  hotkey_weight;

        bool    should_block;    // level >= WARN 권장 차단 여부
    };

    // 메인 진입점 — 모듈별 점수 받아서 최종 판정 반환.
    //   tm_score    : ThreatMatcher 세션 점수 (0~10)
    //   url_score   : UrlClassifier 점수 (미구현 → 0 전달)
    //   typing_score: TypingAnalyzer 점수 (미구현 → 0 전달)
    //   hotkey_score: HotkeyScorer.final_score (없으면 0)
    Verdict Combine(double tm_score,
                     double url_score     = 0.0,
                     double typing_score  = 0.0,
                     double hotkey_score  = 0.0);

    // 검증/UI/디버깅용 — Verdict 를 사람 읽을 수 있는 문자열로
    std::wstring FormatVerdict(const Verdict& v);

    // Level → 한글 라벨 변환
    const wchar_t* LevelName(Level lv);
}
