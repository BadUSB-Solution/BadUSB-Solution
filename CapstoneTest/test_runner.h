#pragma once
#include "keyboard_hook.h"
#include "key_decoder.h"
#include "shell_detector.h"
#include "threat_matcher.h"
#include "url_classifier.h"
#include "typing_analyzer.h"
#include "hotkey_scorer.h"
#include "compound_scorer.h"
#include <string>
#include <vector>

namespace TestRunner
{
    struct PipelineResult {
        KeyDecoder::DecodeResult       decoded;
        ShellDetector::Result          shell;
        ThreatMatcher::ThreatResult    threat;
        UrlClassifier::Result          url;
        TypingAnalyzer::Result         typing;
        HotkeyScorer::HotkeyResult     hotkey;
        CompoundScorer::Verdict        verdict;
    };

    // 알고리즘 파이프라인 전체 실행
    PipelineResult Run(const std::vector<KeyboardHook::KeyRecord>& keys);

    // 결과를 콘솔용 문자열로 변환
    std::wstring FormatResult(const std::vector<KeyboardHook::KeyRecord>& keys,
                               const PipelineResult& r);
}
