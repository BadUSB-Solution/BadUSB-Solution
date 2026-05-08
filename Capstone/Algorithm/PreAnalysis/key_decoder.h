#pragma once
#include "framework.h"
#include "keyboard_hook.h"
#include <vector>
#include <string>

// KeyDecoder: PreAnalysis 파이프라인 코디네이터
// KeyNormalizer -> (UrlExtractor) -> DecodeResult 반환
namespace KeyDecoder
{
    struct HotkeyEvent {
        std::wstring combo;       // "Win+R", "Win+X", "Ctrl+S", "Alt+F4" 등 (locale 독립)
        wchar_t      submenuKey;  // Win+X 서브메뉴 키 ('A', 'J' 등). 서브메뉴 없으면 0
        size_t       lineIndex;
    };

    struct UrlInfo {
        std::wstring raw;        // 원문 (Chr() 포함 가능)
        std::wstring resolved;   // Chr() 재조합 + URL 디코딩 완료본
        size_t       lineIndex;
    };

    struct DecodedLine {
        std::wstring text;              // 정규화된 한 줄 텍스트
        std::vector<UrlInfo> urls;      // 이 줄에서 발견된 URL 목록
    };

    struct ParsedCommand {
        std::wstring              command;   // 소문자 정규화된 명령어 (첫 토큰)
        std::vector<std::wstring> args;      // 원본 케이스 유지 인자 목록
        size_t                    lineIndex;
    };

    struct DecodeResult {
        std::vector<DecodedLine>    lines;    // Enter 기준 분할 줄
        std::vector<HotkeyEvent>    hotkeys;  // 감지된 단축키 이벤트
        std::vector<ParsedCommand>  commands; // 명령어 파싱 결과
        std::wstring                fullText; // 전체 텍스트 (\n 구분)
    };

    // VkCode 버퍼 전체를 분석해 DecodeResult 반환
    DecodeResult Decode(const std::vector<KeyboardHook::KeyRecord>& buf);
}
