#pragma once
#include "url_classifier.h"
#include "url_parser.h"

// 규칙 1: IP 주소 직접 접근.
// host 가 IPv4 형식이면 URL_FLAG_IP_ADDRESS 설정.
// IP 종류(loopback/private/linklocal)에 따라 세부 플래그 추가.
namespace UrlClassifier::IpFilter
{
    uint32_t Check(const ParsedUrl& p);
}
