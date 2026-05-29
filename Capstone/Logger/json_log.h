#pragma once
#include <string>

namespace JsonLog
{
    void Add(const std::wstring& device,
             const std::wstring& code,
             const std::wstring& detail,
             double              score   = -1.0,
             const std::wstring& verdict = L"");

    // Drop entries older than `retentionDays`. retentionDays <= 0 means keep forever.
    // Persists the trimmed log to disk.
    void Prune(int retentionDays);
}
