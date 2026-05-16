#pragma once
#include "url_classifier.h"
#include "url_parser.h"

// 규칙 3: path 끝이 실행파일 확장자인지 검사.
namespace UrlClassifier::ExecutableFilter
{
    uint32_t Check(const ParsedUrl& p);
}
