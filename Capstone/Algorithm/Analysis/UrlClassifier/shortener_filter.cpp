#include "shortener_filter.h"

namespace UrlClassifier::ShortenerFilter
{
    static const wchar_t* SHORTENER_DOMAINS[] = {
        // 주요 단축 URL 서비스
        L"bit.ly",       L"tinyurl.com", L"t.co",         L"goo.gl",
        L"is.gd",        L"buff.ly",     L"adf.ly",       L"ow.ly",
        L"shorturl.at",  L"rb.gy",       L"cutt.ly",      L"rebrand.ly",
        L"tiny.cc",      L"url.kr",      L"vo.la",        L"han.gl",
        // 페이스트빈 계열 (페이로드 호스팅에 자주 사용)
        L"pastebin.com", L"hastebin.com", L"paste.ee",
    };

    uint32_t Check(const ParsedUrl& p)
    {
        if (p.host.empty()) return 0;
        for (auto* d : SHORTENER_DOMAINS) {
            if (MatchesSuffix(p.host, d))
                return URL_FLAG_SHORTENER;
        }
        return 0;
    }
}
