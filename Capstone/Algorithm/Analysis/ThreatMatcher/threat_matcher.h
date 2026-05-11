#pragma once
#include "framework.h"
#include "key_decoder.h"
#include <string>
#include <vector>
#include <set>

// ──────────────────────────────────────────────────────────────────────
// ThreatMatcher
//   - DecodeResult (PreAnalysis 산출) + ShellDetector 결과를 입력 받아
//     명령어 / 인자 / enum 위협값 매칭 후 ThreatResult 반환.
//   - 점수 산정 정책: 점수체계_통합본.md 참조 (Layer 0~4 + 세션 √N 합산).
//   - DB: matchingDB.db (SQLite). SetDatabasePath 로 경로 변경 가능.
//   - shell_id: ShellDetector::Shell enum 사용 (0=NONE, 1=CMD, 2=POWERSHELL)
//
// 의존성:
//   - sqlite3 라이브러리 (sqlite3.h / sqlite3.c — vcpkg 또는 amalgamation)
// ──────────────────────────────────────────────────────────────────────
namespace ThreatMatcher
{
    // 토큰 단위 매칭 결과 (param + value 페어 1개)
    struct TokenMatch {
        std::wstring param;        // 매개변수 이름 ('' 면 위치 인자)
        std::wstring value;        // 값 (없으면 '')
        std::wstring tag_key;      // 매칭된 정규 태그 ('' 면 미매칭)
        wchar_t      axis;         // S/C/I/A/X
        double       base_score;   // tags.base_score
        double       enum_adjust;  // enum_threats.score_adjust
        double       token_score;  // max(0, base + enum_adjust)
        bool         from_common;  // common_parameters 에서 매칭됐나
        bool         matched;      // 정규 태그 매칭 성공 (axis ≠ X)
        bool         in_db;        // parameters/common_parameters 에 존재 (axis 무관)
    };

    // 명령어 단위 결과
    //   phase: 어느 분석 단계에서 추가됐는지 (FormatLog 표시용)
    //     1 = Phase 1   — KeyDecoder 가 line 별로 추출한 정상 명령
    //     4 = Phase 4   — fullText 정규식 보완 스캔 (SPEC §5.5)
    //     5 = Phase 5   — cmd /c "..." / powershell -Command "..." 안 unquote 후 재분석
    struct CommandMatch {
        std::wstring command;
        size_t       lineIndex;
        bool         in_db;         // commands 또는 alias 에서 매칭됨
        int          phase;         // 1 / 4 / 5
        std::vector<TokenMatch> tokens;
        double       cmd_raw;       // Σ token_score (cap 적용 안 함)
    };

    // 세션 단위 최종 결과 — CompoundScorer 로 전달됨
    struct ThreatResult {
        std::vector<CommandMatch>  commands;
        std::set<std::wstring>     activated_tags;  // 세션에서 활성된 정규 태그
        int                         axis_count_s;
        int                         axis_count_c;
        int                         axis_count_i;
        int                         axis_count_a;
        int                         total_tokens;
        int                         matched_tokens;
        int                         unmatched_tokens;
        double                      session_raw;     // Σ cmd_raw
        double                      session_score;   // min(10, raw / √N_categories)
    };

    // DB 경로 설정 (호출 안 하면 기본 경로 사용)
    void SetDatabasePath(const std::wstring& path);

    // 첫 실행 시 exe 옆의 matchingDB.db 를 %APPDATA%\AbangAll\ 로 이동 (copy + delete).
    // - 이미 AppData 에 있으면 아무 것도 안 함
    // - 둘 다 없으면 false (DB 누락 — 호출 측에서 에러 처리)
    // - 호출 후 자동으로 SetDatabasePath(AppData 경로) 적용됨
    // 앱 startup 어딘가 (Policy::Load 직후 권장) 에서 1회 호출.
    bool EnsureDatabaseInstalled();

    // 메인 진입점
    //   shell_id: ShellDetector::Detect(decoded).shell 값을 그대로 전달
    ThreatResult Analyze(const KeyDecoder::DecodeResult& decoded, int shell_id = 0);

    // 앱 종료 시 호출 (DB 연결 해제)
    void Shutdown();

    // 검증/디버깅용 — 사람이 읽을 수 있는 로그 문자열
    std::wstring FormatLog(const ThreatResult& result);
}
