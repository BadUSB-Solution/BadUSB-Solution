// Policy JSON DB — fingerprint 기반 화이트/블랙리스트 관리
// 저장 형식: { "whitelist": [...], "blacklist": [...] }
// 각 항목: { "vid": "046D", "pid": "C52B", "name": "Logitech USB Receiver", "fingerprint": "0x046DC52B1A2B3C4D" }

#include "policy.h"
#include <windows.h>
#include <shlobj.h>

#pragma comment(lib, "Shell32.lib")

#include <fstream>
#include <sstream>
#include <iomanip>
#include <mutex>
#include <algorithm>
#include <cwchar>

namespace
{
    std::mutex g_mu;
    std::vector<UsbItem> g_whitelist;
    std::vector<UsbItem> g_blacklist;
    std::wstring g_dbPath;

    // ── 유틸 ──

    std::wstring GetDefaultDbPath()
    {
        wchar_t appData[MAX_PATH]{};
        if (SHGetFolderPathW(nullptr, CSIDL_APPDATA, nullptr, 0, appData) != S_OK)
        {
            // fallback: 환경변수
            DWORD len = GetEnvironmentVariableW(L"APPDATA", appData, MAX_PATH);
            if (len == 0 || len >= MAX_PATH)
                wcscpy_s(appData, L"C:\\Temp");
        }

        std::wstring dir(appData);
        dir += L"\\AbangAll";

        // 폴더 없으면 생성
        CreateDirectoryW(dir.c_str(), nullptr);

        return dir + L"\\allow_list.json";
    }

    // wstring ↔ UTF-8
    std::string WtoU8(const std::wstring& ws)
    {
        if (ws.empty()) return {};
        int len = WideCharToMultiByte(CP_UTF8, 0, ws.c_str(), (int)ws.size(), nullptr, 0, nullptr, nullptr);
        std::string out(len, '\0');
        WideCharToMultiByte(CP_UTF8, 0, ws.c_str(), (int)ws.size(), &out[0], len, nullptr, nullptr);
        return out;
    }

    std::wstring U8toW(const std::string& s)
    {
        if (s.empty()) return {};
        int len = MultiByteToWideChar(CP_UTF8, 0, s.c_str(), (int)s.size(), nullptr, 0);
        std::wstring out(len, L'\0');
        MultiByteToWideChar(CP_UTF8, 0, s.c_str(), (int)s.size(), &out[0], len);
        return out;
    }

    // uint16_t → "046D" (4자리 hex)
    std::string HexU16(uint16_t v)
    {
        std::ostringstream ss;
        ss << std::uppercase << std::setfill('0') << std::setw(4) << std::hex << v;
        return ss.str();
    }

    // uint64_t → "0x046DC52B1A2B3C4D"
    std::string HexU64(uint64_t v)
    {
        std::ostringstream ss;
        ss << "0x" << std::uppercase << std::setfill('0') << std::setw(16) << std::hex << v;
        return ss.str();
    }

    uint16_t ParseHexU16(const std::string& s)
    {
        return static_cast<uint16_t>(strtoul(s.c_str(), nullptr, 16));
    }

    uint64_t ParseHexU64(const std::string& s)
    {
        // "0x..." 접두어 처리
        const char* p = s.c_str();
        if (s.size() > 2 && s[0] == '0' && (s[1] == 'x' || s[1] == 'X'))
            p += 2;
        return strtoull(p, nullptr, 16);
    }

    // ── JSON 문자열 이스케이프 ──

    std::string JsonEscape(const std::string& s)
    {
        std::string out;
        out.reserve(s.size() + 8);
        for (char ch : s) {
            switch (ch) {
            case '"':  out += "\\\""; break;
            case '\\': out += "\\\\"; break;
            case '\b': out += "\\b";  break;
            case '\f': out += "\\f";  break;
            case '\n': out += "\\n";  break;
            case '\r': out += "\\r";  break;
            case '\t': out += "\\t";  break;
            default:   out += ch;     break;
            }
        }
        return out;
    }

    std::string JsonUnescape(const std::string& s)
    {
        std::string out;
        out.reserve(s.size());
        for (size_t i = 0; i < s.size(); i++) {
            if (s[i] == '\\' && i + 1 < s.size()) {
                char next = s[++i];
                switch (next) {
                case '"':  out += '"';  break;
                case '\\': out += '\\'; break;
                case 'b':  out += '\b'; break;
                case 'f':  out += '\f'; break;
                case 'n':  out += '\n'; break;
                case 'r':  out += '\r'; break;
                case 't':  out += '\t'; break;
                default:   out += '\\'; out += next; break;
                }
            }
            else {
                out += s[i];
            }
        }
        return out;
    }

    // ── 항목 직렬화 ──

    std::string ItemToJson(const UsbItem& item, int indent)
    {
        std::string pad(indent, ' ');
        std::string pad2(indent + 4, ' ');
        std::ostringstream ss;
        ss << pad << "{\n";
        ss << pad2 << "\"vid\": \""         << HexU16(item.vid)                     << "\",\n";
        ss << pad2 << "\"pid\": \""         << HexU16(item.pid)                     << "\",\n";
        ss << pad2 << "\"name\": \""        << JsonEscape(WtoU8(item.name))         << "\",\n";
        ss << pad2 << "\"fingerprint\": \"" << HexU64(item.fingerprint)             << "\"\n";
        ss << pad << "}";
        return ss.str();
    }

    std::string ListToJsonArray(const std::vector<UsbItem>& items, int indent)
    {
        std::string pad(indent, ' ');
        if (items.empty())
            return "[]";

        std::ostringstream ss;
        ss << "[\n";
        for (size_t i = 0; i < items.size(); i++) {
            ss << ItemToJson(items[i], indent + 4);
            if (i + 1 < items.size()) ss << ",";
            ss << "\n";
        }
        ss << pad << "]";
        return ss.str();
    }

    // ── 간이 JSON 파서 (오브젝트 배열 전용) ──

    void SkipWhitespace(const std::string& s, size_t& pos)
    {
        while (pos < s.size() && (s[pos] == ' ' || s[pos] == '\t' || s[pos] == '\n' || s[pos] == '\r'))
            pos++;
    }

    // "..." 를 읽어서 내용만 반환 (이스케이프 처리)
    bool ReadJsonString(const std::string& s, size_t& pos, std::string& out)
    {
        SkipWhitespace(s, pos);
        if (pos >= s.size() || s[pos] != '"') return false;
        pos++; // skip opening "

        std::string raw;
        while (pos < s.size() && s[pos] != '"') {
            if (s[pos] == '\\' && pos + 1 < s.size()) {
                raw += s[pos];
                raw += s[pos + 1];
                pos += 2;
            }
            else {
                raw += s[pos++];
            }
        }
        if (pos >= s.size()) return false;
        pos++; // skip closing "

        out = JsonUnescape(raw);
        return true;
    }

    // { "vid":"...", "pid":"...", "name":"...", "fingerprint":"..." }
    bool ParseItem(const std::string& s, size_t& pos, UsbItem& item)
    {
        SkipWhitespace(s, pos);
        if (pos >= s.size() || s[pos] != '{') return false;
        pos++; // skip {

        while (true) {
            SkipWhitespace(s, pos);
            if (pos >= s.size()) return false;
            if (s[pos] == '}') { pos++; return true; }

            std::string key, value;
            if (!ReadJsonString(s, pos, key)) return false;

            SkipWhitespace(s, pos);
            if (pos >= s.size() || s[pos] != ':') return false;
            pos++;

            if (!ReadJsonString(s, pos, value)) return false;

            if (key == "vid")              item.vid = ParseHexU16(value);
            else if (key == "pid")         item.pid = ParseHexU16(value);
            else if (key == "name")        item.name = U8toW(value);
            else if (key == "fingerprint") item.fingerprint = ParseHexU64(value);

            SkipWhitespace(s, pos);
            if (pos < s.size() && s[pos] == ',') pos++;
        }
    }

    // [...] 배열 파싱
    bool ParseArray(const std::string& s, size_t& pos, std::vector<UsbItem>& items)
    {
        SkipWhitespace(s, pos);
        if (pos >= s.size() || s[pos] != '[') return false;
        pos++;

        while (true) {
            SkipWhitespace(s, pos);
            if (pos >= s.size()) return false;
            if (s[pos] == ']') { pos++; return true; }

            UsbItem item;
            if (!ParseItem(s, pos, item)) return false;
            items.push_back(item);

            SkipWhitespace(s, pos);
            if (pos < s.size() && s[pos] == ',') pos++;
        }
    }

    // 최상위: { "whitelist": [...], "blacklist": [...] }
    bool ParseDb(const std::string& s, std::vector<UsbItem>& wl, std::vector<UsbItem>& bl)
    {
        size_t pos = 0;
        SkipWhitespace(s, pos);
        if (pos >= s.size() || s[pos] != '{') return false;
        pos++;

        while (true) {
            SkipWhitespace(s, pos);
            if (pos >= s.size()) return false;
            if (s[pos] == '}') return true;

            std::string key;
            if (!ReadJsonString(s, pos, key)) return false;

            SkipWhitespace(s, pos);
            if (pos >= s.size() || s[pos] != ':') return false;
            pos++;

            if (key == "whitelist") {
                if (!ParseArray(s, pos, wl)) return false;
            }
            else if (key == "blacklist") {
                if (!ParseArray(s, pos, bl)) return false;
            }
            else {
                // 알 수 없는 키 → 값 스킵 (배열 또는 문자열)
                SkipWhitespace(s, pos);
                if (pos < s.size() && s[pos] == '[') {
                    std::vector<UsbItem> dummy;
                    if (!ParseArray(s, pos, dummy)) return false;
                }
                else {
                    std::string dummy;
                    if (!ReadJsonString(s, pos, dummy)) return false;
                }
            }

            SkipWhitespace(s, pos);
            if (pos < s.size() && s[pos] == ',') pos++;
        }
    }

    // 리스트에서 중복 확인
    bool Contains(const std::vector<UsbItem>& list, uint64_t fp)
    {
        return std::any_of(list.begin(), list.end(),
            [fp](const UsbItem& i) { return i.fingerprint == fp; });
    }

    bool HasVidPid(uint16_t vid, uint16_t pid)
    {
        return vid != 0 || pid != 0;
    }

    // 블랙리스트는 VID/PID 단위 차단을 지원한다.
    // 같은 모델/수신기 계열 장치를 다음 연결부터 함께 막기 위한 기준이다.
    bool ContainsVidPid(const std::vector<UsbItem>& list, uint16_t vid, uint16_t pid)
    {
        if (!HasVidPid(vid, pid)) {
            return false;
        }

        return std::any_of(list.begin(), list.end(),
            [vid, pid](const UsbItem& i) {
                return i.vid == vid && i.pid == pid;
            });
    }

    // whitelist와 blacklist가 동시에 맞는 충돌 상태를 만들지 않도록
    // fingerprint 또는 VID/PID가 같은 기존 항목을 반대 목록에서 제거한다.
    void RemoveByFingerprintOrVidPid(std::vector<UsbItem>& list, const UsbItem& item)
    {
        list.erase(
            std::remove_if(list.begin(), list.end(),
                [&](const UsbItem& i) {
                    const bool sameFingerprint = i.fingerprint == item.fingerprint;
                    const bool sameVidPid =
                        HasVidPid(item.vid, item.pid) &&
                        i.vid == item.vid &&
                        i.pid == item.pid;
                    return sameFingerprint || sameVidPid;
                }),
            list.end());
    }
}

// ═══════════════════════════════════════════════════
//  Policy 네임스페이스 구현
// ═══════════════════════════════════════════════════

namespace Policy
{
    void SetDbPath(const std::wstring& path)
    {
        std::lock_guard<std::mutex> lk(g_mu);
        g_dbPath = path;
    }

    std::wstring GetDbPath()
    {
        std::lock_guard<std::mutex> lk(g_mu);
        if (g_dbPath.empty())
            g_dbPath = GetDefaultDbPath();
        return g_dbPath;
    }

    bool Load()
    {
        const std::wstring path = GetDbPath();

        std::ifstream ifs(path, std::ios::binary);
        if (!ifs.is_open())
            return false; // 파일 없으면 빈 상태로 시작

        std::ostringstream ss;
        ss << ifs.rdbuf();
        const std::string json = ss.str();
        ifs.close();

        std::vector<UsbItem> wl, bl;
        if (!ParseDb(json, wl, bl))
            return false;

        // allow 플래그 설정
        for (auto& item : wl) item.allow = true;
        for (auto& item : bl) item.allow = false;

        std::lock_guard<std::mutex> lk(g_mu);
        g_whitelist = std::move(wl);
        g_blacklist = std::move(bl);
        return true;
    }

    bool Save()
    {
        std::string json;
        {
            std::lock_guard<std::mutex> lk(g_mu);
            std::ostringstream ss;
            ss << "{\n";
            ss << "    \"whitelist\": " << ListToJsonArray(g_whitelist, 4) << ",\n";
            ss << "    \"blacklist\": " << ListToJsonArray(g_blacklist, 4) << "\n";
            ss << "}\n";
            json = ss.str();
        }

        const std::wstring path = GetDbPath();
        std::ofstream ofs(path, std::ios::binary | std::ios::trunc);
        if (!ofs.is_open())
            return false;

        ofs << json;
        ofs.close();
        return ofs.good();
    }

    PolicyStatus Evaluate(const UsbItem& item)
    {
        std::lock_guard<std::mutex> lk(g_mu);
        // 차단 정책은 보수적으로 우선 적용한다.
        // fingerprint가 달라도 같은 VID/PID로 등록된 blacklist면 차단한다.
        if (Contains(g_blacklist, item.fingerprint) ||
            ContainsVidPid(g_blacklist, item.vid, item.pid)) {
            return PolicyStatus::Blacklisted;
        }

        // 허용은 fingerprint 기준으로만 처리한다.
        // 같은 VID/PID의 다른 실제 장치를 자동 허용하지 않기 위해서다.
        // (ContainerId 부실로 fingerprint가 변동되는 DIY/프로토타입 보드 류는
        //  BadUSB 후보군과 겹치므로 매번 승인을 받게 두는 편이 의도에 맞다.)
        if (Contains(g_whitelist, item.fingerprint)) {
            return PolicyStatus::Whitelisted;
        }

        return PolicyStatus::Unknown;
    }

    void AddWhitelist(const UsbItem& item)
    {
        {
            std::lock_guard<std::mutex> lk(g_mu);
            // 허용으로 바꾸는 경우 기존 차단 정책과 충돌하지 않게 먼저 제거한다.
            RemoveByFingerprintOrVidPid(g_blacklist, item);

            if (!Contains(g_whitelist, item.fingerprint)) {
                UsbItem copy = item;
                copy.allow = true;
                g_whitelist.push_back(copy);
            }
        }
        Save();
    }

    void AddBlacklist(const UsbItem& item)
    {
        {
            std::lock_guard<std::mutex> lk(g_mu);
            // 차단으로 바꾸는 경우 기존 허용 정책과 충돌하지 않게 먼저 제거한다.
            RemoveByFingerprintOrVidPid(g_whitelist, item);

            if (!Contains(g_blacklist, item.fingerprint) &&
                !ContainsVidPid(g_blacklist, item.vid, item.pid)) {
                UsbItem copy = item;
                copy.allow = false;
                g_blacklist.push_back(copy);
            }
        }
        Save();
    }

    void Remove(uint64_t fp)
    {
        {
            std::lock_guard<std::mutex> lk(g_mu);
            g_whitelist.erase(
                std::remove_if(g_whitelist.begin(), g_whitelist.end(),
                    [fp](const UsbItem& i) { return i.fingerprint == fp; }),
                g_whitelist.end());
            g_blacklist.erase(
                std::remove_if(g_blacklist.begin(), g_blacklist.end(),
                    [fp](const UsbItem& i) { return i.fingerprint == fp; }),
                g_blacklist.end());
        }
        Save();
    }

    std::vector<UsbItem> GetWhitelist()
    {
        std::lock_guard<std::mutex> lk(g_mu);
        return g_whitelist;
    }

    std::vector<UsbItem> GetBlacklist()
    {
        std::lock_guard<std::mutex> lk(g_mu);
        return g_blacklist;
    }
}
