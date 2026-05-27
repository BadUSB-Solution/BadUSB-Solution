#pragma once
#include "keyboard_hook.h"
#include <string>
#include <vector>

// "Win R N O T E P A D Enter" 같은 토큰 시퀀스를
// KeyboardHook::KeyRecord 벡터로 변환.
//
// 토큰 종류:
//   Modifier (one-shot): Win, Ctrl, Alt, Shift
//   Named:               Space, Enter, Tab, Backspace, Esc, Delete, Insert,
//                        Home, End, PgUp, PgDn, Up, Down, Left, Right, F1~F12
//   Letter:              A~Z (대소문자 무관)
//   Digit:               0~9
//   Symbol:              - = [ ] \ ; ' , . / `
//
// Modifier 동작:
//   Modifier 토큰이 나오면 "다음 키" 까지만 적용. 그 다음 키 직전 keydown,
//   직후 keyup 으로 emit. KeyNormalizer 의 modifier 상태 추적 로직과 호환.
namespace TestKeyParser
{
    struct Options {
        DWORD startTime  = 0;
        DWORD intervalMs = 50;  // 인터키 간격 (BadUSB 시뮬: 5, 사람: 80~150)
    };

    std::vector<KeyboardHook::KeyRecord>
    ParseSequence(const std::wstring& tokens, const Options& opts = {});
}
