#pragma once
#include "url_classifier.h"
#include "url_parser.h"

// 규칙 6: 명시된 포트가 표준 포트(80/443/8080/8443) 외이면 의심.
// 포트 명시 안된 경우(-1) 는 정상으로 처리.
namespace UrlClassifier::PortFilter
{
    uint32_t Check(const ParsedUrl& p);
}
