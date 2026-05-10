#pragma once
#include "framework.h"
#include "../../PreAnalysis/key_decoder.h"
#include <string>
#include <vector>

// ──────────────────────────────────────────────────────────────────────
// HotkeyScorer
//   KeyDecoder::DecodeResult.hotkeys 를 입력받아 위협 점수 산출.
//   ThreatMatcher / UrlClassifier 와 동일하게 CompoundScorer 가 가중 평균에 사용.
//
// 설계 의도:
//   - BadUSB 는 거의 항상 핫키로 시작 (Win+R, Ctrl+Shift+Enter 등).
//     ThreatMatcher 는 commands + args 만 분석하므로 핫키 시그널이 누락됨.
//   - 핫키 자체는 정상 사용에서도 등장 (사용자가 직접 누르는 경우).
//     따라서 조합 점수 + (선택) 빠른 시퀀스 가중치로 위협도 산정.
//
// 호출 패턴 (CompoundScorer 와 결합):
//   auto hk = HotkeyScorer::Analyze(decoded.hotkeys);
//   auto verdict = CompoundScorer::Combine(tm.session_score, 0, 0, hk.final_score);
// ──────────────────────────────────────────────────────────────────────
namespace HotkeyScorer
{
    // 매칭된 핫키 1건
    struct HotkeyMatch {
        std::wstring combo;       // 입력 그대로 (예: "Win+R", "Ctrl+Shift+Enter")
        std::wstring tag_key;     // 분류 라벨 (디버깅 용도, 스코어에 영향 X)
        size_t       lineIndex;
        double       score;       // 이 1건이 기여하는 점수
    };

    // 세션 단위 핫키 점수 결과
    struct HotkeyResult {
        std::vector<HotkeyMatch> matches;
        double                   raw_sum;       // Σ matches[i].score
        double                   final_score;   // min(SCORE_CAP, raw_sum), 0~10
    };

    // 메인 진입점
    //   hotkeys: KeyDecoder::DecodeResult.hotkeys 그대로 전달
    HotkeyResult Analyze(const std::vector<KeyDecoder::HotkeyEvent>& hotkeys);

    // 디버깅/UI 용 — 사람이 읽을 수 있는 로그
    std::wstring FormatLog(const HotkeyResult& r);
}
