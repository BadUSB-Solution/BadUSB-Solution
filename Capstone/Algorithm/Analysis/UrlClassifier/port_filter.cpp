#include "port_filter.h"

namespace UrlClassifier::PortFilter
{
    uint32_t Check(const ParsedUrl& p)
    {
        if (p.port < 0) return 0;  // 명시 안됨 → 정상
        if (p.port == 80 || p.port == 443 ||
            p.port == 8080 || p.port == 8443) return 0;
        return URL_FLAG_UNUSUAL_PORT;
    }
}
