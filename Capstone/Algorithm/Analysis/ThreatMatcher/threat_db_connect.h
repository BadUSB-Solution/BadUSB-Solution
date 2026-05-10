#pragma once
#include <string>
#include <vector>

// ──────────────────────────────────────────────────────────────────────
// matchingDB.db lookup 레이어.
// SQLite 연결을 한 번 열고 재사용. 실패하면 false 반환 (silent).
// ──────────────────────────────────────────────────────────────────────
namespace ThreatMatcher { namespace DB {
    // 매개변수 lookup 결과
    struct ParamRow {
        bool         found;
        std::wstring tag_key;     // '' 이면 미매칭 행
        wchar_t      axis;        // S/C/I/A/X
        double       base_score;  // 0 if unmatched
    };

    // DB 연결 관리
    void SetPath(const std::wstring& path);
    bool Open();           // 이미 열려있으면 true 반환
    void Close();

    // ─── lookup 함수들 ──────────────────────────────────────────────
    // shell_id 우선 시도, 미스 시 다른 shell fallback.
    // commands → command_aliases 순.
    //
    // multi-word 명령 지원 (reg add, sc query 등 23개):
    //   next_token 이 비어있지 않으면 "cmd_name + ' ' + next_token" 도 시도.
    //   매칭되면 *used_next = true 로 caller 에 알림 (caller 가 args[0] 소비 처리).
    //
    // 반환: command_id (-1 = 미발견)
    int LookupCommand(const std::wstring& cmd_name, int shell_id,
                      const std::wstring& next_token = L"",
                      bool* used_next = nullptr);

    // command_id + 매개변수 → ParamRow (매칭 인자 우선, 없으면 미매칭)
    ParamRow LookupParameter(int command_id, const std::wstring& param);

    // 글로벌 PowerShell 공통 매개변수 (cmdlet 무관)
    ParamRow LookupCommonParameter(const std::wstring& param);

    // command-self row (parameter_name='') 들 — 인자 없는 명령어용
    std::vector<ParamRow> LookupCommandSelf(int command_id);

    // enum 위협값 가산점 (없으면 0)
    //   command-specific 우선 → 글로벌 fallback
    double LookupEnumAdjust(const std::wstring& cmd_name,
                             const std::wstring& param,
                             const std::wstring& value);

    // 디버깅 — command_id 로 cmdlet 이름 조회
    std::wstring GetCommandName(int command_id);
}}  // namespace ThreatMatcher::DB
