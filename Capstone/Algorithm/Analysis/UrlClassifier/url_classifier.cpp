#include "url_classifier.h"
#include "url_parser.h"
#include "ip_filter.h"
#include "shortener_filter.h"
#include "executable_filter.h"
#include "entropy_filter.h"
#include "scheme_filter.h"
#include "port_filter.h"
#include "hosting_filter.h"

namespace UrlClassifier
{
    Result Classify(const KeyDecoder::DecodeResult& decoded)
    {
        Result result;
        result.aggregateFlags = 0;

        for (const auto& line : decoded.lines) {
            for (const auto& info : line.urls) {
                ParsedUrl p = ParseUrl(info.resolved);
                uint32_t flags = 0;

                flags |= IpFilter::Check(p);
                flags |= ShortenerFilter::Check(p);
                flags |= ExecutableFilter::Check(p);
                flags |= EntropyFilter::Check(p);
                flags |= SchemeFilter::Check(p);
                flags |= PortFilter::Check(p);
                flags |= HostingFilter::Check(p);

                UrlClassification c;
                c.url       = info.resolved;
                c.lineIndex = info.lineIndex;
                c.flags     = flags;
                result.items.push_back(std::move(c));
                result.aggregateFlags |= flags;
            }
        }

        return result;
    }
}
