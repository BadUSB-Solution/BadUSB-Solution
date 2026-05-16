#pragma once

#include "threat_matcher.h"

namespace ThreatMatcher::Internal
{
    CommandMatch AnalyzeCommand(const KeyDecoder::ParsedCommand& parsed, int shell_id);

    void Phase4FullTextScan(
        const KeyDecoder::DecodeResult& decoded,
        int shell_id,
        ThreatResult& result);

    void Phase5UnquoteAndReanalyze(
        int shell_id,
        ThreatResult& result,
        int depth,
        size_t scan_start = 0);

    void AggregateSession(ThreatResult& result);

    std::wstring FormatResultLog(const ThreatResult& result);
}
