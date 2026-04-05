#pragma once
#include "framework.h"
#include "keyboard_hook.h"
#include <vector>
#include <string>

// KeyNormalizer: VkCode 버퍼 -> 텍스트 줄 + 단축키 이벤트
// Phase 4 PreAnalysis 파이프라인 1단계
namespace KeyNormalizer
{
    struct HotkeyEvent {
        std::wstring combo;      // "Win+R", "Alt+Enter" 등
        size_t       lineIndex;  // 발생 당시 줄 인덱스
    };

    struct NormalizeResult {
        std::vector<std::wstring> lines;    // Enter 기준 분할된 줄
        std::vector<HotkeyEvent>  hotkeys;
    };

    // VkCode 버퍼 -> 정규화된 줄 목록 + 단축키 이벤트
    // - Win/Alt + Enter  -> HotkeyEvent (줄바꿈 안 함)
    // - Ctrl/Shift/단독 Enter -> 줄바꿈
    // - 나머지 키는 현재 단계에서는 그대로 통과 (이후 문자 변환 추가 예정)
    NormalizeResult Normalize(const std::vector<KeyboardHook::KeyRecord>& buf);
}
