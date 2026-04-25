#pragma once
#include "framework.h"
#include "key_decoder.h"
#include <vector>
#include <string>

// UrlExtractor: 정규화된 줄 목록에서 URL 추출 + 디코딩
// Phase 4 PreAnalysis 파이프라인 2단계
//
// 처리 순서 (줄 단위):
//   ① 직접 URL 탐지:    https?://[^\s"'<>]+
//   ② Chr() 치환 후 탐지: Chr(58)→: Chr(47)→/ 등 치환 후 재탐지
//   ③ 변수 누적 평가:   VBScript/PowerShell 변수 대입·연결 추적
//      ex) u="https"&Chr(58)&Chr(47)&Chr(47) / u=u&"bit.ly/xxx"
//   ④ 퍼센트 디코딩:   추출된 URL의 %XX → 문자 (UrlInfo.resolved 에 저장)
namespace UrlExtractor
{
    // lines[i].urls 를 in-place 로 채움 (lines[i].text 는 읽기 전용)
    // 변수 누적 컨텍스트는 내부에서 줄 순서대로 유지됨
    void Extract(std::vector<KeyDecoder::DecodedLine>& lines);
}
