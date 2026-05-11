#pragma once
#include "url_classifier.h"
#include "url_parser.h"

// 규칙 7~11: 무료/위장 호스팅 카테고리 (5종 통합).
//  - URL_FLAG_FREE_TLD       (.tk, .ml, .ga, .cf, .gq)
//  - URL_FLAG_FREE_HOSTING   (github.io, netlify.app 등)
//  - URL_FLAG_DYNAMIC_DNS    (duckdns.org 등)
//  - URL_FLAG_FILE_SHARING   (mega.nz 등)
//  - URL_FLAG_TUNNELING      (ngrok.io 등)
namespace UrlClassifier::HostingFilter
{
    uint32_t Check(const ParsedUrl& p);
}
