#include "threat_db_connect.h"
#include "sqlite3.h"
#include <Windows.h>
#include <mutex>

namespace ThreatMatcher {
namespace DB {

// ─── 내부 상태 + 헬퍼 (anonymous namespace) ─────────────────────────
namespace {

    sqlite3*     g_db   = nullptr;
    // 기본 DB 경로 — 비어 있으면 Open() 시 ResolveDefaultPath() 가 채움.
    // 배포 시에는 SetPath() 또는 EnsureDatabaseInstalled() 로 재지정 가능.
    std::wstring g_path;
    std::mutex   g_mutex;

    // exe 가 있는 폴더 + "matchingDB.db" 를 default 경로로 사용.
    //   - 빌드 후 출력 폴더 (예: x64/Debug/) 에 matchingDB.db 가 같이 있어야 함.
    //   - 개발 중에는 vcxproj post-build event 로 Core\matchingDB.db 를 출력 폴더로 복사.
    //   - 배포 시에는 exe 와 같은 폴더에 함께 패키징.
    std::wstring ResolveDefaultPath()
    {
        wchar_t buf[MAX_PATH];
        DWORD n = GetModuleFileNameW(nullptr, buf, MAX_PATH);
        if (n == 0 || n >= MAX_PATH) return L"matchingDB.db";  // CWD fallback
        std::wstring path(buf, n);
        auto pos = path.find_last_of(L"\\/");
        if (pos == std::wstring::npos) return L"matchingDB.db";
        return path.substr(0, pos + 1) + L"matchingDB.db";
    }

    // ─── UTF-16 ↔ UTF-8 ─────────────────────────────────────────
    std::string WideToUtf8(const std::wstring& w)
    {
        if (w.empty()) return std::string();
        int n = WideCharToMultiByte(CP_UTF8, 0, w.data(), (int)w.size(),
                                     nullptr, 0, nullptr, nullptr);
        std::string s((size_t)n, 0);
        WideCharToMultiByte(CP_UTF8, 0, w.data(), (int)w.size(),
                             &s[0], n, nullptr, nullptr);
        return s;
    }

    std::wstring Utf8ToWide(const char* s, int len)
    {
        if (!s || !*s) return std::wstring();
        if (len < 0) len = (int)strlen(s);
        int n = MultiByteToWideChar(CP_UTF8, 0, s, len, nullptr, 0);
        std::wstring w((size_t)n, 0);
        MultiByteToWideChar(CP_UTF8, 0, s, len, &w[0], n);
        return w;
    }

    // SQL prepare 헬퍼
    sqlite3_stmt* Prepare(const char* sql)
    {
        sqlite3_stmt* stmt = nullptr;
        if (sqlite3_prepare_v2(g_db, sql, -1, &stmt, nullptr) != SQLITE_OK)
            return nullptr;
        return stmt;
    }

    void BindWStr(sqlite3_stmt* stmt, int idx, const std::wstring& w)
    {
        std::string utf8 = WideToUtf8(w);
        sqlite3_bind_text(stmt, idx, utf8.c_str(), (int)utf8.size(),
                           SQLITE_TRANSIENT);
    }

    std::wstring ColumnWStr(sqlite3_stmt* stmt, int col)
    {
        const unsigned char* txt = sqlite3_column_text(stmt, col);
        int len = sqlite3_column_bytes(stmt, col);
        if (!txt || len == 0) return std::wstring();
        return Utf8ToWide(reinterpret_cast<const char*>(txt), len);
    }

} // anonymous namespace


// ─── 연결 관리 ─────────────────────────────────────────────────
void SetPath(const std::wstring& path) { g_path = path; }

bool Open()
{
    std::lock_guard<std::mutex> lock(g_mutex);
    if (g_db) return true;
    // SetPath() 으로 미리 지정 안 했으면 exe 옆 matchingDB.db 사용
    if (g_path.empty()) g_path = ResolveDefaultPath();
    std::string utf8 = WideToUtf8(g_path);
    int rc = sqlite3_open_v2(utf8.c_str(), &g_db,
                              SQLITE_OPEN_READONLY, nullptr);
    if (rc != SQLITE_OK) {
        sqlite3_close(g_db);
        g_db = nullptr;
        return false;
    }
    return true;
}

void Close()
{
    std::lock_guard<std::mutex> lock(g_mutex);
    if (g_db) {
        sqlite3_close(g_db);
        g_db = nullptr;
    }
}

// ─── 내부: 단일 lookup (prefix stripping 없이 raw) ─────────────
namespace {
    int LookupCommandRaw(const std::wstring& cmd_name, int shell_id)
    {
        if (cmd_name.empty()) return -1;

        int order[2];
        if (shell_id == 1) { order[0] = 1; order[1] = 2; }
        else               { order[0] = 2; order[1] = 1; }

        for (int sid : order) {
            sqlite3_stmt* stmt = Prepare(
                "SELECT command_id FROM commands "
                "WHERE shell_id = ? AND command_name = ? COLLATE NOCASE LIMIT 1");
            if (stmt) {
                sqlite3_bind_int(stmt, 1, sid);
                BindWStr(stmt, 2, cmd_name);
                int cid = -1;
                if (sqlite3_step(stmt) == SQLITE_ROW)
                    cid = sqlite3_column_int(stmt, 0);
                sqlite3_finalize(stmt);
                if (cid > 0) return cid;
            }

            stmt = Prepare(
                "SELECT canonical_id FROM command_aliases "
                "WHERE shell_id = ? AND alias_name = ? COLLATE NOCASE LIMIT 1");
            if (stmt) {
                sqlite3_bind_int(stmt, 1, sid);
                BindWStr(stmt, 2, cmd_name);
                int cid = -1;
                if (sqlite3_step(stmt) == SQLITE_ROW)
                    cid = sqlite3_column_int(stmt, 0);
                sqlite3_finalize(stmt);
                if (cid > 0) return cid;
            }
        }
        return -1;
    }

    // SPEC §4.1 보완 — 제어구조 prefix 제거.
    //   "(new-object" → "new-object"
    //   "if(test-path" → "test-path"
    //   "while(condition-x" → "condition-x"
    //   '(' 위치 찾아서 그 뒤가 알파벳/cmd 형태면 그 뒷부분 반환.
    std::wstring StripControlPrefix(const std::wstring& s)
    {
        auto pos = s.find(L'(');
        if (pos == std::wstring::npos) return std::wstring();
        if (pos + 1 >= s.size()) return std::wstring();
        wchar_t next = s[pos + 1];
        bool is_word = (next >= L'a' && next <= L'z')
                    || (next >= L'A' && next <= L'Z')
                    || (next == L'_');
        if (!is_word) return std::wstring();
        return s.substr(pos + 1);
    }
}

// ─── PowerShell 변수 할당 prefix 제거 ─────────────────────────────
//   "$cert=New-SelfSignedCertificate" → "New-SelfSignedCertificate"
//   PS 의 변수 할당 (`$var=cmd args...`) 은 우측 cmd 가 실제 실행됨.
//   따라서 분석 대상이 맞고, prefix 만 떼어내고 cmd lookup 진행.
//
//   안전 조건 (오인 방지):
//   - $varname 이 합법적 식별자 (영숫자/_/콜론) 이고
//   - '=' 다음 첫 글자가 알파벳/언더바 (= cmdlet/command 후보)
//   위 만족하면 prefix 제거. 그 외 (`$h='string'`, `$x=$env:VAR`,
//   `$y=(...)`, `$z=@{...}` 등) 는 표현식이라 cmd 아님 → strip 안 함.
namespace {
    std::wstring StripVarAssignPrefix(const std::wstring& s)
    {
        if (s.size() < 3 || s[0] != L'$') return std::wstring();
        // '=' 위치 찾기 (변수 이름 안에는 = 안 들어감)
        auto eq = s.find(L'=');
        if (eq == std::wstring::npos || eq + 1 >= s.size()) return std::wstring();
        // 변수 이름 부분 검증: $ 다음부터 = 직전까지 모두 식별자 문자
        for (size_t i = 1; i < eq; ++i) {
            wchar_t c = s[i];
            bool ok = (c >= L'a' && c <= L'z') || (c >= L'A' && c <= L'Z')
                   || (c >= L'0' && c <= L'9') || c == L'_' || c == L':';
            if (!ok) return std::wstring();
        }
        // '=' 다음 첫 문자가 cmd 후보인지
        wchar_t first = s[eq + 1];
        bool is_cmd_start = (first >= L'a' && first <= L'z')
                         || (first >= L'A' && first <= L'Z')
                         || first == L'_';
        if (!is_cmd_start) return std::wstring();
        return s.substr(eq + 1);
    }
}

// ─── LookupCommand: .exe strip + $var= strip + multi-word + control prefix ────
int LookupCommand(const std::wstring& cmd_name, int shell_id,
                  const std::wstring& next_token, bool* used_next)
{
    if (used_next) *used_next = false;
    if (!g_db && !Open()) return -1;

    // 1) 원본 그대로 시도 (single-word)
    int cid = LookupCommandRaw(cmd_name, shell_id);
    if (cid > 0) return cid;

    // 2) .exe suffix 제거 후 재시도 (case-insensitive)
    //    DB 는 'sc', 'taskkill', 'wmic' 등 suffix 없는 형태로만 저장.
    //    실제 입력은 'sc.exe', 'taskkill.exe' 처럼 쓰는 경우가 많음.
    std::wstring after_exe;  // multi-word 검사에도 재사용
    if (cmd_name.size() > 4) {
        auto suf = cmd_name.substr(cmd_name.size() - 4);
        if (_wcsicmp(suf.c_str(), L".exe") == 0) {
            after_exe = cmd_name.substr(0, cmd_name.size() - 4);
            cid = LookupCommandRaw(after_exe, shell_id);
            if (cid > 0) return cid;
        }
    }

    // 3) PS 변수 할당 prefix 제거 후 재시도
    //    "$cert=New-SelfSignedCertificate" → "New-SelfSignedCertificate"
    std::wstring after_var = StripVarAssignPrefix(cmd_name);
    if (!after_var.empty()) {
        cid = LookupCommandRaw(after_var, shell_id);
        if (cid > 0) return cid;
    }

    // 4) multi-word 시도: "cmd_name next_token" (+ .exe / $var= 제거형)
    //    DB 에 reg add / sc query / ... 같은 23개 multi-word commands 존재.
    //    next_token 이 flag (-, /, +) 로 시작하면 multi-word 일 리 없으니 skip.
    if (!next_token.empty()) {
        wchar_t c0 = next_token[0];
        bool nt_is_flag = (c0 == L'-' || c0 == L'/' || c0 == L'+');
        if (!nt_is_flag) {
            // 우선: 원본 + next_token
            std::wstring combined = cmd_name + L" " + next_token;
            cid = LookupCommandRaw(combined, shell_id);
            if (cid > 0) {
                if (used_next) *used_next = true;
                return cid;
            }
            // 보완: .exe strip 형 + next_token (예: "sc.exe stop" → "sc stop")
            if (!after_exe.empty()) {
                combined = after_exe + L" " + next_token;
                cid = LookupCommandRaw(combined, shell_id);
                if (cid > 0) {
                    if (used_next) *used_next = true;
                    return cid;
                }
            }
            // 보완: $var= strip 형 + next_token
            if (!after_var.empty()) {
                combined = after_var + L" " + next_token;
                cid = LookupCommandRaw(combined, shell_id);
                if (cid > 0) {
                    if (used_next) *used_next = true;
                    return cid;
                }
            }
        }
    }

    // 5) SPEC §4.1 — 제어구조 prefix 제거 후 재시도
    //    "(new-object", "if(test-path" 같은 토큰 보완
    std::wstring stripped = StripControlPrefix(cmd_name);
    if (!stripped.empty()) {
        cid = LookupCommandRaw(stripped, shell_id);
        if (cid > 0) return cid;
    }
    return -1;
}

// ─── LookupParameter ────────────────────────────────────────────
ParamRow LookupParameter(int command_id, const std::wstring& param)
{
    ParamRow r{ false, std::wstring(), L'X', 0.0 };
    if (!g_db && !Open()) return r;

    sqlite3_stmt* stmt = Prepare(
        "SELECT t.tag_key, p.axis, COALESCE(t.base_score, 0) "
        "FROM parameters p "
        "LEFT JOIN tags t ON p.tag_id = t.tag_id "
        "WHERE p.command_id = ? AND p.parameter_name = ? "
        "ORDER BY (p.axis = 'X') "
        "LIMIT 1");
    if (!stmt) return r;
    sqlite3_bind_int(stmt, 1, command_id);
    BindWStr(stmt, 2, param);

    if (sqlite3_step(stmt) == SQLITE_ROW) {
        r.found      = true;
        r.tag_key    = ColumnWStr(stmt, 0);
        std::wstring ax = ColumnWStr(stmt, 1);
        r.axis       = ax.empty() ? L'X' : ax[0];
        r.base_score = sqlite3_column_double(stmt, 2);
    }
    sqlite3_finalize(stmt);
    return r;
}

// ─── LookupCommonParameter ──────────────────────────────────────
ParamRow LookupCommonParameter(const std::wstring& param)
{
    ParamRow r{ false, std::wstring(), L'X', 0.0 };
    if (!g_db && !Open()) return r;

    sqlite3_stmt* stmt = Prepare(
        "SELECT t.tag_key, t.axis, t.base_score "
        "FROM common_parameters cp JOIN tags t ON cp.tag_id = t.tag_id "
        "WHERE cp.param_name = ? COLLATE NOCASE LIMIT 1");
    if (!stmt) return r;
    BindWStr(stmt, 1, param);

    if (sqlite3_step(stmt) == SQLITE_ROW) {
        r.found      = true;
        r.tag_key    = ColumnWStr(stmt, 0);
        std::wstring ax = ColumnWStr(stmt, 1);
        r.axis       = ax.empty() ? L'X' : ax[0];
        r.base_score = sqlite3_column_double(stmt, 2);
    }
    sqlite3_finalize(stmt);
    return r;
}

// ─── LookupCommandSelf (parameter_name='') ──────────────────────
std::vector<ParamRow> LookupCommandSelf(int command_id)
{
    std::vector<ParamRow> out;
    if (!g_db && !Open()) return out;

    sqlite3_stmt* stmt = Prepare(
        "SELECT t.tag_key, t.axis, t.base_score "
        "FROM parameters p JOIN tags t ON p.tag_id = t.tag_id "
        "WHERE p.command_id = ? AND p.parameter_name = ''");
    if (!stmt) return out;
    sqlite3_bind_int(stmt, 1, command_id);

    while (sqlite3_step(stmt) == SQLITE_ROW) {
        ParamRow r;
        r.found      = true;
        r.tag_key    = ColumnWStr(stmt, 0);
        std::wstring ax = ColumnWStr(stmt, 1);
        r.axis       = ax.empty() ? L'X' : ax[0];
        r.base_score = sqlite3_column_double(stmt, 2);
        out.push_back(std::move(r));
    }
    sqlite3_finalize(stmt);
    return out;
}

// ─── LookupEnumAdjust ───────────────────────────────────────────
double LookupEnumAdjust(const std::wstring& cmd_name,
                         const std::wstring& param,
                         const std::wstring& value)
{
    if (value.empty()) return 0.0;
    if (!g_db && !Open()) return 0.0;

    // 1) command-specific
    if (!cmd_name.empty()) {
        sqlite3_stmt* stmt = Prepare(
            "SELECT score_adjust FROM enum_threats "
            "WHERE command_name = ? AND parameter_name = ? AND value_name = ? "
            "LIMIT 1");
        if (stmt) {
            BindWStr(stmt, 1, cmd_name);
            BindWStr(stmt, 2, param);
            BindWStr(stmt, 3, value);
            double adj = 0.0;
            if (sqlite3_step(stmt) == SQLITE_ROW)
                adj = sqlite3_column_double(stmt, 0);
            sqlite3_finalize(stmt);
            if (adj != 0.0) return adj;
        }
    }

    // 2) global (command_name IS NULL)
    sqlite3_stmt* stmt = Prepare(
        "SELECT score_adjust FROM enum_threats "
        "WHERE command_name IS NULL AND parameter_name = ? AND value_name = ? "
        "LIMIT 1");
    if (!stmt) return 0.0;
    BindWStr(stmt, 1, param);
    BindWStr(stmt, 2, value);
    double adj = 0.0;
    if (sqlite3_step(stmt) == SQLITE_ROW)
        adj = sqlite3_column_double(stmt, 0);
    sqlite3_finalize(stmt);
    return adj;
}

// ─── 디버깅 헬퍼 ──────────────────────────────────────────────
std::wstring GetCommandName(int command_id)
{
    if (!g_db && !Open()) return std::wstring();
    sqlite3_stmt* stmt = Prepare(
        "SELECT command_name FROM commands WHERE command_id = ?");
    if (!stmt) return std::wstring();
    sqlite3_bind_int(stmt, 1, command_id);
    std::wstring name;
    if (sqlite3_step(stmt) == SQLITE_ROW) name = ColumnWStr(stmt, 0);
    sqlite3_finalize(stmt);
    return name;
}

}} // namespace ThreatMatcher::DB
