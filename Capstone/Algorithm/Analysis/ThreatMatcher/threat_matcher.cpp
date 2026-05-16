#include "threat_matcher.h"

#include "threat_db_connect.h"
#include "threat_matcher_internal.h"

namespace ThreatMatcher
{
    void SetDatabasePath(const std::wstring& path)
    {
        DB::SetPath(path);
    }

    void Shutdown()
    {
        DB::Close();
    }

    ThreatResult Analyze(const KeyDecoder::DecodeResult& decoded, int shell_id)
    {
        ThreatResult result{};
        if (!DB::Open()) {
            return result;
        }

        result.commands.reserve(decoded.commands.size());
        for (const auto& parsed : decoded.commands) {
            result.commands.push_back(Internal::AnalyzeCommand(parsed, shell_id));
        }

        Internal::Phase4FullTextScan(decoded, shell_id, result);
        Internal::Phase5UnquoteAndReanalyze(shell_id, result, 0);
        Internal::AggregateSession(result);
        return result;
    }

    std::wstring FormatLog(const ThreatResult& result)
    {
        return Internal::FormatResultLog(result);
    }
}
