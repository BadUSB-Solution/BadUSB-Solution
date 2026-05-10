#pragma once
#include <windows.h>

// ──────────────────────────────────────────────────────────────────────
// CompoundScorerUI
//   현재 키보드 버퍼를 즉시 분석해서 ThreatMatcher 상세 + 최종 위협 점수
//   (CompoundScorer Verdict) 를 한 화면에 표시.
//
// 흐름:
//   사용자 트리거 (예: BufferView 의 [Analyze] 버튼)
//     ↓
//   KeyboardHook::GetBufferedKeys()
//     ↓ KeyDecoder::Decode
//     ↓ ShellDetector::Detect
//     ↓ ThreatMatcher::Analyze
//     ↓ CompoundScorer::Combine
//     ↓
//   결과를 두 영역 (Verdict / ThreatMatcher 상세) 으로 출력
// ──────────────────────────────────────────────────────────────────────
namespace CompoundScorerUI
{
    // 모달리스 분석 창 열기 (이미 열려 있으면 최상위로 + 새로고침)
    void Show(HWND hParent = nullptr);

    // 명시적 닫기 (앱 종료 시 등)
    void Close();
}
