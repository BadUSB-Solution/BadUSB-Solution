#pragma once
#include "framework.h"
#include <string>

// UrlClassifier 내부에서 공유하는 URL 컴포넌트 분해 + 문자열 헬퍼.
// UrlExtractor가 추출한 URL 문자열(UrlInfo.resolved)을 받아서 scheme/host/port/path로 분리.
namespace UrlClassifier
{
    struct ParsedUrl {
        std::wstring scheme;    // "https" (소문자)
        std::wstring host;      // "bit.ly" 또는 "192.168.1.1" (소문자)
        int          port;      // -1 = 명시 안됨, 그 외 = 명시된 포트
        std::wstring path;      // "/47OkUuD" (원본 케이스)
    };

    // URL 문자열 → ParsedUrl 분해
    ParsedUrl ParseUrl(const std::wstring& url);

    // ----- 공통 문자열 헬퍼 -----
    std::wstring ToLower(const std::wstring& s);
    bool         EndsWith(const std::wstring& s, const std::wstring& suffix);

    // host 가 domain 과 정확히 같거나, "*.domain" 형태이면 true.
    // 예: MatchesSuffix("sub.bit.ly", "bit.ly") = true
    //     MatchesSuffix("evil-bit.ly", "bit.ly") = false
    bool MatchesSuffix(const std::wstring& host, const std::wstring& domain);

    // wide → narrow (ASCII 범위만 안전). 호스트/스킴 등 영문 비교용.
    std::string  WideToNarrow(const std::wstring& s);
}
