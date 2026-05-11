#pragma once
#include "url_classifier.h"
#include "url_parser.h"

// 규칙 5: 스킴이 http/https 아니면 비정상.
// file://, ftp://, smb://, gopher://, javascript:, data: 등.
namespace UrlClassifier::SchemeFilter
{
    uint32_t Check(const ParsedUrl& p);
}
