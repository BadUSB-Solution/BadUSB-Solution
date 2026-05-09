#include "threat_db_connect.h"
#include "sqlite3.h"
#include <Windows.h>
#include <mutex>

namespace ThreatMatcher {
namespace DB {

// ─── 내부 상태 + 헬퍼 (anonymous namespace) ─────────────────────────
namespace {

    sqlite3*     g_db   = nullptr;
    std::wstring g_path = L"C:\\Users\\hanwoo\\Desktop\\mapping\\matchingDB.db";
    std::mutex   g_mutex;

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

// ─── LookupCommand: shell_id 우선, fallback 으로 다른 shell ─────
int LookupCommand(const std::wstring& cmd_name, int shell_id)
{
    if (!g_db && !Open()) return -1;

    int order[2];
    if (shell_id == 1) { order[0] = 1; order[1] = 2; }
    else               { order[0] = 2; order[1] = 1; }

    for (int sid : order) {
        // 1) commands 직접 매칭
        sqlite3_stmt* stmt = Prepare(
            "SELECT command_id FROM commands "
            "WHERE shell_id = ? AND command_name = ? COLLATE NOCASE LIMIT 1");
        if (!stmt) continue;
        sqlite3_bind_int(stmt, 1, sid);
        BindWStr(stmt, 2, cmd_name);
        int cid = -1;
        if (sqlite3_step(stmt) == SQLITE_ROW)
            cid = sqlite3_column_int(stmt, 0);
        sqlite3_finalize(stmt);
        if (cid > 0) return cid;

        // 2) command_aliases lookup
        stmt = Prepare(
            "SELECT canonical_id FROM command_aliases "
            "WHERE shell_id = ? AND alias_name = ? COLLATE NOCASE LIMIT 1");
        if (!stmt) continue;
        sqlite3_bind_int(stmt, 1, sid);
        BindWStr(stmt, 2, cmd_name);
        if (sqlite3_step(stmt) == SQLITE_ROW)
            cid = sqlite3_column_int(stmt, 0);
        sqlite3_finalize(stmt);
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
