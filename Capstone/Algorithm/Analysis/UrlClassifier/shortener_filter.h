#pragma once
#include "url_classifier.h"
#include "url_parser.h"

// 규칙 2: 단축 URL 서비스 도메인 감지.
// 알려진 단축 도메인 화이트리스트와 suffix 매칭.
namespace UrlClassifier::ShortenerFilter
{
    uint32_t Check(const ParsedUrl& p);
}
