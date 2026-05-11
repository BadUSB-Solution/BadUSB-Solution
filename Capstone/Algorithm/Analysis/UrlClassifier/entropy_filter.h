#pragma once
#include "url_classifier.h"
#include "url_parser.h"

// 규칙 4: DGA / 고엔트로피 도메인 감지.
// 도메인 메인 라벨의 Shannon entropy 가 임계값 초과 시 의심.
namespace UrlClassifier::EntropyFilter
{
    uint32_t Check(const ParsedUrl& p);
}
