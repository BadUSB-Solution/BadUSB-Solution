#pragma once
#include <cstdint>

namespace Policy
{
    bool IsWhitelisted(uint64_t fp);
    bool IsBlacklisted(uint64_t fp);
    void AddWhitelist(uint64_t fp);
    void AddBlacklist(uint64_t fp);
}
