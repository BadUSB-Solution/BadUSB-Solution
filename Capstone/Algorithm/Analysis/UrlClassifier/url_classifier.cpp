#include "url_classifier.h"
#include "url_parser.h"
#include "ip_filter.h"
#include "shortener_filter.h"
#include "executable_filter.h"
#include "entropy_filter.h"
#include "scheme_filter.h"
#include "port_filter.h"
#include "hosting_filter.h"

#include <algorithm>

namespace UrlClassifier
{
    // ──────────────────────────────────────────────────────────────────
    // 점수 정책 (임시값 — calibration 시 조정)
    //
    // 설계 원칙:
    //   - 단독 신호로 강한 것 (FREE_TLD, EXE_EXTENSION 등) → 높음
    //   - 정상 사용 흔한 것 (FREE_HOSTING github.io 등) → 낮음 (조합으로만 의미)
    //   - IP 세부 플래그는 IP_ADDRESS 위에 가산
    // ──────────────────────────────────────────────────────────────────
    static constexpr double SCORE_CAP = 10.0;

    // 플래그별 가중치
    static constexpr double W_IP_ADDRESS       = 3.0;
    static constexpr double W_IP_PRIVATE       = 1.0;  // 내부망 공격
    static constexpr double W_IP_LOOPBACK      = 0.5;  // 로컬 (BadUSB 관련 약함)
    static constexpr double W_IP_LINKLOCAL     = 1.5;  // 169.254 비정상

    static constexpr double W_SHORTENER        = 2.5;  // bit.ly 등 — 경로 은닉
    static constexpr double W_EXE_EXTENSION    = 2.5;  // .exe / .ps1 등 직접 다운로드
    static constexpr double W_HIGH_ENTROPY     = 2.0;  // DGA 의심
    static constexpr double W_ABNORMAL_SCHEME  = 3.0;  // file:// ftp:// 등
    static constexpr double W_UNUSUAL_PORT     = 1.5;  // 비표준 포트

    static constexpr double W_FREE_TLD         = 3.5;  // .tk .ml 등 — 거의 무조건 악성
    static constexpr double W_FREE_HOSTING     = 1.0;  // github.io 등 — 단독 약함
    static constexpr double W_DYNAMIC_DNS      = 2.5;  // duckdns 등 — C2 흔적
    static constexpr double W_FILE_SHARING     = 1.5;  // mega.nz 등
    static constexpr double W_TUNNELING        = 2.5;  // ngrok 등 — 임시 노출

    double ScoreFlags(uint32_t f)
    {
        double s = 0.0;
        if (f & URL_FLAG_IP_ADDRESS)       s += W_IP_ADDRESS;
        if (f & URL_FLAG_IP_PRIVATE)       s += W_IP_PRIVATE;
        if (f & URL_FLAG_IP_LOOPBACK)      s += W_IP_LOOPBACK;
        if (f & URL_FLAG_IP_LINKLOCAL)     s += W_IP_LINKLOCAL;

        if (f & URL_FLAG_SHORTENER)        s += W_SHORTENER;
        if (f & URL_FLAG_EXE_EXTENSION)    s += W_EXE_EXTENSION;
        if (f & URL_FLAG_HIGH_ENTROPY)     s += W_HIGH_ENTROPY;
        if (f & URL_FLAG_ABNORMAL_SCHEME)  s += W_ABNORMAL_SCHEME;
        if (f & URL_FLAG_UNUSUAL_PORT)     s += W_UNUSUAL_PORT;

        if (f & URL_FLAG_FREE_TLD)         s += W_FREE_TLD;
        if (f & URL_FLAG_FREE_HOSTING)     s += W_FREE_HOSTING;
        if (f & URL_FLAG_DYNAMIC_DNS)      s += W_DYNAMIC_DNS;
        if (f & URL_FLAG_FILE_SHARING)     s += W_FILE_SHARING;
        if (f & URL_FLAG_TUNNELING)        s += W_TUNNELING;

        return (std::min)(SCORE_CAP, s);
    }

    Result Classify(const KeyDecoder::DecodeResult& decoded)
    {
        Result result;
        result.aggregateFlags = 0;
        result.final_score = 0.0;

        double rawSum = 0.0;

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
                c.score     = ScoreFlags(flags);

                rawSum += c.score;
                result.aggregateFlags |= flags;
                result.items.push_back(std::move(c));
            }
        }

        result.final_score = (std::min)(SCORE_CAP, rawSum);
        return result;
    }
}
