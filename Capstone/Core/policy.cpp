// 해당 부분에서 화이트 리스트나 블랙 리스트 등을 관리할 예정

#include "policy.h"
#include <unordered_set>

namespace Policy
{
    static std::unordered_set<uint64_t> g_whitelist;
    static std::unordered_set<uint64_t> g_blacklist;

    bool IsWhitelisted(uint64_t fp) { return g_whitelist.count(fp) != 0; }
    bool IsBlacklisted(uint64_t fp) { return g_blacklist.count(fp) != 0; }
    void AddWhitelist(uint64_t fp) { g_whitelist.insert(fp); }
    void AddBlacklist(uint64_t fp) { g_blacklist.insert(fp); }
}
