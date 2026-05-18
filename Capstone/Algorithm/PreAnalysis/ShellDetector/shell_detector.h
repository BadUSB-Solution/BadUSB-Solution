#pragma once
#include "framework.h"
#include "key_decoder.h"

// ShellDetector: 셸 오프너 감지
// DecodeResult를 분석해 어떤 셸이 열렸는지 판정한다.
// ThreatMatcher가 이 값으로 CMD DB / PowerShell DB 중 어느 쪽을 쓸지 결정.
//
// 한계 사항은 LIMITATIONS.md 참고.
namespace ShellDetector
{
    enum Shell : int {
        SHELL_NONE       = 0,   // 셸 미감지
        SHELL_CMD        = 1,   // Windows CMD
        SHELL_POWERSHELL = 2,   // PowerShell / pwsh / PS ISE
    };

    struct Result {
        Shell  shell;       // 감지된 셸 종류
        size_t lineIndex;   // 셸 오프너 감지 위치 (없으면 0)
        bool   elevated;    // 관리자 권한 시도 여부 (Win+X→A 등)
    };

    // DecodeResult 분석 → 셸 종류 판정
    Result Detect(const KeyDecoder::DecodeResult& decoded);
}
