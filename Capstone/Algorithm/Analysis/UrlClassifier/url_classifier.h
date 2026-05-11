#pragma once
#include "framework.h"
#include "key_decoder.h"
#include <string>
#include <vector>
#include <cstdint>

// UrlClassifier: 추출된 URL을 11가지 규칙으로 분류
// 입력: KeyDecoder::DecodeResult.lines[i].urls[j].resolved
// 출력: URL별 비트마스크 + 전체 OR
//
// 자세한 명세는 URL_CLASSIFIER_SPEC.md 참고.
namespace UrlClassifier
{
    enum UrlFlag : uint32_t {
        URL_FLAG_NONE             = 0,

        // 규칙 1: IP 주소 (세분화)
        URL_FLAG_IP_ADDRESS       = 1u << 0,
        URL_FLAG_IP_PRIVATE       = 1u << 1,
        URL_FLAG_IP_LOOPBACK      = 1u << 2,
        URL_FLAG_IP_LINKLOCAL     = 1u << 3,

        // 규칙 2: 단축 URL
        URL_FLAG_SHORTENER        = 1u << 4,

        // 규칙 3: 실행파일 확장자
        URL_FLAG_EXE_EXTENSION    = 1u << 5,

        // 규칙 4: 고엔트로피 도메인 (DGA 의심)
        URL_FLAG_HIGH_ENTROPY     = 1u << 6,

        // 규칙 5: 비정상 스킴
        URL_FLAG_ABNORMAL_SCHEME  = 1u << 7,

        // 규칙 6: 비표준 포트
        URL_FLAG_UNUSUAL_PORT     = 1u << 8,

        // 규칙 7~11: 무료/위장 호스팅 (세분화)
        URL_FLAG_FREE_TLD         = 1u << 9,    // .tk, .ml, .ga, .cf, .gq
        URL_FLAG_FREE_HOSTING     = 1u << 10,   // github.io, netlify.app 등
        URL_FLAG_DYNAMIC_DNS      = 1u << 11,   // duckdns.org 등
        URL_FLAG_FILE_SHARING     = 1u << 12,   // mega.nz 등
        URL_FLAG_TUNNELING        = 1u << 13,   // ngrok.io 등
    };

    struct UrlClassification {
        std::wstring url;        // resolved URL
        size_t       lineIndex;
        uint32_t     flags;      // UrlFlag 비트마스크
    };

    struct Result {
        std::vector<UrlClassification>  items;
        uint32_t                         aggregateFlags;  // 전체 OR
    };

    Result Classify(const KeyDecoder::DecodeResult& decoded);
}
