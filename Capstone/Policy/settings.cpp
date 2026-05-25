// 사용자 환경설정 settings.json — flat key/value (bool + string).
// 저장 형식:
//   {
//       "autoStart": false,
//       "minimizeToTray": true,
//       "unknownDeviceAction": "approval",
//       "behaviorMonitoring": true,
//       "notifyOnRisk": true
//   }

#include "settings.h"
#include "app_paths.h"

#include <fstream>
#include <sstream>
#include <mutex>

namespace
{
    std::mutex g_mu;
    Policy::Settings g_settings;
    std::wstring g_path;

    std::wstring GetPath()
    {
        if (g_path.empty()) g_path = Storage::GetSettingsPath();
        return g_path;
    }

    // ── enum ↔ 문자열 ──

    const char* ActionToString(Policy::UnknownDeviceAction a)
    {
        switch (a) {
        case Policy::UnknownDeviceAction::AutoBlock: return "autoBlock";
        case Policy::UnknownDeviceAction::AutoAllow: return "autoAllow";
        case Policy::UnknownDeviceAction::Approval:
        default:                                     return "approval";
        }
    }

    Policy::UnknownDeviceAction ActionFromString(const std::string& s)
    {
        if (s == "autoBlock") return Policy::UnknownDeviceAction::AutoBlock;
        if (s == "autoAllow") return Policy::UnknownDeviceAction::AutoAllow;
        return Policy::UnknownDeviceAction::Approval;
    }

    // ── 미니 JSON 파서 (flat: bool + string) ──

    void SkipWs(const std::string& s, size_t& p)
    {
        while (p < s.size() && (s[p] == ' ' || s[p] == '\t' || s[p] == '\n' || s[p] == '\r'))
            p++;
    }

    bool ReadString(const std::string& s, size_t& p, std::string& out)
    {
        SkipWs(s, p);
        if (p >= s.size() || s[p] != '"') return false;
        p++;
        out.clear();
        while (p < s.size() && s[p] != '"') {
            if (s[p] == '\\' && p + 1 < s.size()) {
                // 간이 이스케이프: \" 와 \\ 만 처리
                out += s[p + 1];
                p += 2;
            }
            else {
                out += s[p++];
            }
        }
        if (p >= s.size()) return false;
        p++;
        return true;
    }

    bool ReadBool(const std::string& s, size_t& p, bool& out)
    {
        SkipWs(s, p);
        if (s.compare(p, 4, "true") == 0)  { out = true;  p += 4; return true; }
        if (s.compare(p, 5, "false") == 0) { out = false; p += 5; return true; }
        return false;
    }

    // 값 스킵 (알 수 없는 키 만났을 때): 다음 콤마나 } 까지 단순 진행.
    void SkipValue(const std::string& s, size_t& p)
    {
        SkipWs(s, p);
        if (p < s.size() && s[p] == '"') {
            // 문자열은 짝 맞춰 스킵
            p++;
            while (p < s.size() && s[p] != '"') {
                if (s[p] == '\\' && p + 1 < s.size()) p += 2;
                else p++;
            }
            if (p < s.size()) p++;
            return;
        }
        while (p < s.size() && s[p] != ',' && s[p] != '}') p++;
    }

    bool ParseSettings(const std::string& s, Policy::Settings& out)
    {
        size_t p = 0;
        SkipWs(s, p);
        if (p >= s.size() || s[p] != '{') return false;
        p++;

        while (true) {
            SkipWs(s, p);
            if (p >= s.size()) return false;
            if (s[p] == '}') return true;

            std::string key;
            if (!ReadString(s, p, key)) return false;

            SkipWs(s, p);
            if (p >= s.size() || s[p] != ':') return false;
            p++;

            if      (key == "autoStart")           ReadBool(s, p, out.autoStart);
            else if (key == "minimizeToTray")      ReadBool(s, p, out.minimizeToTray);
            else if (key == "behaviorMonitoring")  ReadBool(s, p, out.behaviorMonitoring);
            else if (key == "notifyOnRisk")        ReadBool(s, p, out.notifyOnRisk);
            else if (key == "unknownDeviceAction") {
                std::string v;
                if (ReadString(s, p, v)) out.unknownDeviceAction = ActionFromString(v);
            }
            else {
                SkipValue(s, p);
            }

            SkipWs(s, p);
            if (p < s.size() && s[p] == ',') p++;
        }
    }

    // ── 직렬화 ──

    std::string MakeJson(const Policy::Settings& s)
    {
        auto B = [](bool b) -> const char* { return b ? "true" : "false"; };

        std::ostringstream ss;
        ss << "{\n";
        ss << "    \"autoStart\": "           << B(s.autoStart)                      << ",\n";
        ss << "    \"minimizeToTray\": "      << B(s.minimizeToTray)                 << ",\n";
        ss << "    \"unknownDeviceAction\": \"" << ActionToString(s.unknownDeviceAction) << "\",\n";
        ss << "    \"behaviorMonitoring\": "  << B(s.behaviorMonitoring)             << ",\n";
        ss << "    \"notifyOnRisk\": "        << B(s.notifyOnRisk)                   << "\n";
        ss << "}\n";
        return ss.str();
    }

    bool WriteToDisk(const Policy::Settings& snapshot)
    {
        const std::wstring path = GetPath();
        const std::string json  = MakeJson(snapshot);

        // hidden 파일은 ofstream(trunc)으로 못 열 수 있어 잠시 해제.
        Storage::UnhideForWrite(path);

        std::ofstream ofs(path, std::ios::binary | std::ios::trunc);
        if (!ofs.is_open())
            return false;

        ofs << json;
        ofs.close();
        const bool ok = ofs.good();

        Storage::MarkHidden(path);
        return ok;
    }
}

// ═══════════════════════════════════════════════════
//  Policy 네임스페이스 — Settings 구현
// ═══════════════════════════════════════════════════

namespace Policy
{
    bool LoadSettings()
    {
        const std::wstring path = GetPath();

        std::ifstream ifs(path, std::ios::binary);
        if (!ifs.is_open()) {
            // 파일 없음 → 기본값으로 첫 저장
            Settings snapshot;
            {
                std::lock_guard<std::mutex> lk(g_mu);
                g_settings = Settings{};
                snapshot = g_settings;
            }
            return WriteToDisk(snapshot);
        }

        std::ostringstream ss;
        ss << ifs.rdbuf();
        const std::string json = ss.str();
        ifs.close();

        Settings parsed;
        if (!ParseSettings(json, parsed))
            return false;

        // 기존 파일도 startup에서 한 번 hidden 처리.
        Storage::MarkHidden(path);

        std::lock_guard<std::mutex> lk(g_mu);
        g_settings = parsed;
        return true;
    }

    bool SaveSettings()
    {
        Settings snapshot;
        {
            std::lock_guard<std::mutex> lk(g_mu);
            snapshot = g_settings;
        }
        return WriteToDisk(snapshot);
    }

    Settings GetSettings()
    {
        std::lock_guard<std::mutex> lk(g_mu);
        return g_settings;
    }

    bool SetSettings(const Settings& s)
    {
        Settings snapshot;
        {
            std::lock_guard<std::mutex> lk(g_mu);
            g_settings = s;
            snapshot = g_settings;
        }
        return WriteToDisk(snapshot);
    }
}
