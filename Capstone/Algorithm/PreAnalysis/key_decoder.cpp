#include "key_decoder.h"
#include "key_normalizer.h"
#include "url_extractor.h"

// KeyDecoder: KeyNormalizer + (UrlExtractor) 결과를 통합해 DecodeResult 반환
namespace KeyDecoder
{
    DecodeResult Decode(const std::vector<KeyboardHook::KeyRecord>& buf)
    {
        DecodeResult result;

        // --- 1단계: 정규화 (줄 분리 + 단축키 감지) ---
        auto norm = KeyNormalizer::Normalize(buf);

        // 단축키 이벤트 복사
        for (auto& h : norm.hotkeys)
            result.hotkeys.push_back({ h.combo, h.lineIndex });

        // 줄 목록 구성 + fullText 조합
        std::wstring fullText;
        for (size_t i = 0; i < norm.lines.size(); ++i) {
            DecodedLine dl;
            dl.text = norm.lines[i];
            // URL 추출은 UrlExtractor 구현 후 여기서 호출 예정
            result.lines.push_back(std::move(dl));

            if (!fullText.empty()) fullText += L"\n";
            fullText += norm.lines[i];
        }
        result.fullText = fullText;

        // --- 2단계: URL 추출 ---
        UrlExtractor::Extract(result.lines);

        return result;
    }
}
