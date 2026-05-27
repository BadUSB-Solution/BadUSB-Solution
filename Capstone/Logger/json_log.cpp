#include "json_log.h"

#include <windows.h>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <iterator>
#include <mutex>
#include <sstream>
#include <vector>

namespace
{
    struct Entry
    {
        std::string time;
        std::string device;
        std::string code;
        std::string detail;
        double      score;
        std::string verdict;
    };

    std::mutex          g_mu;
    std::vector<Entry>  g_entries;
    bool                g_loaded = false;

    std::string Utf8(const std::wstring& s)
    {
        if (s.empty()) return {};
        int n = WideCharToMultiByte(CP_UTF8, 0, s.data(), (int)s.size(), nullptr, 0, nullptr, nullptr);
        if (n <= 0) return {};
        std::string out((size_t)n, '\0');
        WideCharToMultiByte(CP_UTF8, 0, s.data(), (int)s.size(), out.data(), n, nullptr, nullptr);
        return out;
    }

    std::wstring OutPath()
    {
        wchar_t buf[MAX_PATH];
        DWORD n = GetModuleFileNameW(nullptr, buf, MAX_PATH);
        std::wstring p = (n && n < MAX_PATH) ? std::wstring(buf, n) : L".";
        size_t slash = p.find_last_of(L"\\/");
        std::wstring dir = (slash == std::wstring::npos) ? L"." : p.substr(0, slash);
        dir += L"\\logs";
        CreateDirectoryW(dir.c_str(), nullptr);
        return dir + L"\\ui_data.json";
    }

    std::string NowText()
    {
        SYSTEMTIME t;
        GetLocalTime(&t);
        char buf[32];
        std::snprintf(buf, sizeof(buf),
            "%04u-%02u-%02u %02u:%02u:%02u",
            t.wYear, t.wMonth, t.wDay, t.wHour, t.wMinute, t.wSecond);
        return buf;
    }

    void Esc(std::string& out, const std::string& s)
    {
        out.push_back('"');
        for (char c : s) {
            switch (c) {
            case '"':  out += "\\\""; break;
            case '\\': out += "\\\\"; break;
            case '\n': out += "\\n";  break;
            case '\r': out += "\\r";  break;
            case '\t': out += "\\t";  break;
            default:
                if ((unsigned char)c < 0x20) {
                    char b[8]; std::snprintf(b, 8, "\\u%04x", (unsigned char)c);
                    out += b;
                } else out.push_back(c);
            }
        }
        out.push_back('"');
    }

    bool ExtractStr(const std::string& s, const char* key, std::string& out)
    {
        std::string pat = "\""; pat += key; pat += "\":";
        size_t p = s.find(pat);
        if (p == std::string::npos) return false;
        p += pat.size();
        while (p < s.size() && (s[p] == ' ' || s[p] == '\t')) ++p;
        if (p >= s.size() || s[p] != '"') return false;
        ++p;
        out.clear();
        while (p < s.size()) {
            char c = s[p];
            if (c == '"') return true;
            if (c == '\\' && p + 1 < s.size()) {
                char n = s[p + 1];
                switch (n) {
                case '"':  out.push_back('"');  break;
                case '\\': out.push_back('\\'); break;
                case '/':  out.push_back('/');  break;
                case 'b':  out.push_back('\b'); break;
                case 'f':  out.push_back('\f'); break;
                case 'n':  out.push_back('\n'); break;
                case 'r':  out.push_back('\r'); break;
                case 't':  out.push_back('\t'); break;
                case 'u':
                    if (p + 5 < s.size()) { p += 4; }
                    break;
                default: out.push_back(n); break;
                }
                p += 2;
            } else {
                out.push_back(c);
                ++p;
            }
        }
        return false;
    }

    bool ExtractNum(const std::string& s, const char* key, double& out)
    {
        std::string pat = "\""; pat += key; pat += "\":";
        size_t p = s.find(pat);
        if (p == std::string::npos) return false;
        p += pat.size();
        while (p < s.size() && (s[p] == ' ' || s[p] == '\t')) ++p;
        if (p >= s.size() || s[p] == '"') return false;
        char* end = nullptr;
        double v = std::strtod(s.c_str() + p, &end);
        if (end == s.c_str() + p) return false;
        out = v;
        return true;
    }

    void LoadExisting()
    {
        std::ifstream f(OutPath().c_str(), std::ios::binary);
        if (!f.is_open()) return;
        std::string s((std::istreambuf_iterator<char>(f)),
                       std::istreambuf_iterator<char>());

        size_t p = 0;
        while ((p = s.find("\"time\":", p)) != std::string::npos) {
            size_t blockEnd = s.find('}', p);
            if (blockEnd == std::string::npos) break;
            std::string block = s.substr(p, blockEnd - p);

            Entry e;
            e.score = -1.0;
            ExtractStr(block, "time",    e.time);
            ExtractStr(block, "device",  e.device);
            ExtractStr(block, "code",    e.code);
            ExtractStr(block, "detail",  e.detail);
            ExtractStr(block, "verdict", e.verdict);
            ExtractNum(block, "score",   e.score);

            if (!e.time.empty()) g_entries.push_back(std::move(e));
            p = blockEnd + 1;
        }
    }

    void WriteFile()
    {
        std::string j = "{\n  \"events\": [";
        for (size_t i = 0; i < g_entries.size(); ++i) {
            const Entry& e = g_entries[i];
            if (i) j += ',';
            j += "\n    { \"time\": ";
            Esc(j, e.time);
            j += ", \"device\": ";  Esc(j, e.device);
            j += ", \"code\": ";    Esc(j, e.code);
            j += ", \"detail\": ";  Esc(j, e.detail);
            if (e.score >= 0) {
                char b[16]; std::snprintf(b, 16, "%.2f", e.score);
                j += ", \"score\": "; j += b;
            }
            if (!e.verdict.empty()) {
                j += ", \"verdict\": "; Esc(j, e.verdict);
            }
            j += " }";
        }
        j += "\n  ]\n}\n";

        std::wstring path = OutPath();
        std::wstring tmp  = path + L".tmp";
        {
            std::ofstream f(tmp.c_str(), std::ios::binary | std::ios::trunc);
            if (!f.is_open()) return;
            f.write(j.data(), (std::streamsize)j.size());
        }
        MoveFileExW(tmp.c_str(), path.c_str(), MOVEFILE_REPLACE_EXISTING);
    }
}

namespace JsonLog
{
    void Add(const std::wstring& device,
             const std::wstring& code,
             const std::wstring& detail,
             double              score,
             const std::wstring& verdict)
    {
        Entry e;
        e.time    = NowText();
        e.device  = Utf8(device);
        e.code    = Utf8(code);
        e.detail  = Utf8(detail);
        e.score   = score;
        e.verdict = Utf8(verdict);

        std::lock_guard<std::mutex> lk(g_mu);
        if (!g_loaded) {
            g_loaded = true;
            LoadExisting();
        }
        g_entries.push_back(std::move(e));
        if (g_entries.size() > 500) g_entries.erase(g_entries.begin());
        WriteFile();
    }
}
