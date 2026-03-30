#pragma once
#include "framework.h"

// CompoundScorer: aggregate all module results into final threat score
// - TypingAnalyzer score
// - ShellDetector score
// - ThreatMatcher score (with category combo weight)
// - UrlClassifier score
// - Final verdict: score, level, reason, should_block
namespace CompoundScorer
{

}
