#include "scheme_filter.h"

namespace UrlClassifier::SchemeFilter
{
    uint32_t Check(const ParsedUrl& p)
    {
        // scheme 이 비어있으면 (스킴 없는 URL) 일단 정상으로 둠
        // ParseUrl 이 scheme 을 항상 lowercase 로 채워줌
        if (p.scheme.empty()) return 0;
        if (p.scheme == L"http" || p.scheme == L"https") return 0;
        return URL_FLAG_ABNORMAL_SCHEME;
    }
}
