// 앱 상에 로그 띄우는 부분 + 영속 저장
//   - 메모리에 500개 ring buffer 유지 (UI Logs 탭에서 GetAll() 로 표시).
//   - %APPDATA%\AbangAll\log.json 에도 동일 내용을 JSON 으로 저장.
//   - 시작 시 기존 log.json 있으면 LoadExisting 으로 메모리에 적재.

#include "logger.h"
#include "obfuscate.h"
#include "app_paths.h"

#include <windows.h>
#include <algorithm>
#include <cstdio>
#include <ctime>
#include <fstream>
#include <iterator>
#include <mutex>
#include <sstream>
#include <vector>

namespace Logger
{
    // ─── 내부 상태 + 헬퍼 (anonymous namespace) ───────────────────────
    namespace {

        struct Entry {
            std::string time;  // "2026-05-27 21:21:07"
            std::string msg;   // UTF-8 메시지 본문
        };

        std::mutex          g_mu;
        std::vector<Entry>  g_entries;
        bool                g_loaded = false;

        HWND g_notifyWnd = nullptr;
        UINT g_notifyMsg = 0;

        // ─── UTF-16 ↔ UTF-8 ────────────────────────────────────────
        std::string Utf8(const std::wstring& s) {
            if (s.empty()) return {};
            int n = WideCharToMultiByte(CP_UTF8, 0, s.data(), (int)s.size(),
                                         nullptr, 0, nullptr, nullptr);
            if (n <= 0) return {};
            std::string out((size_t)n, '\0');
            WideCharToMultiByte(CP_UTF8, 0, s.data(), (int)s.size(),
                                 out.data(), n, nullptr, nullptr);
            return out;
        }

        std::wstring Wide(const std::string& s) {
            if (s.empty()) return {};
            int n = MultiByteToWideChar(CP_UTF8, 0, s.data(), (int)s.size(),
                                         nullptr, 0);
            if (n <= 0) return {};
            std::wstring out((size_t)n, L'\0');
            MultiByteToWideChar(CP_UTF8, 0, s.data(), (int)s.size(),
                                 out.data(), n);
            return out;
        }

        // log.json 절대 경로. AbangAll 폴더 보장 후 파일명만 붙임.
        std::wstring LogPath() {
            return Storage::GetAbangAllDir()
                 + static_cast<const wchar_t*>(AY_OBFUSCATE(L"\\log.json"));
        }

        std::string NowText() {
            SYSTEMTIME t;
            GetLocalTime(&t);
            char buf[32];
            std::snprintf(buf, sizeof(buf),
                AY_OBFUSCATE("%04u-%02u-%02u %02u:%02u:%02u"),
                t.wYear, t.wMonth, t.wDay, t.wHour, t.wMinute, t.wSecond);
            return buf;
        }

        // ─── 미니 JSON 직렬화/파싱 ─────────────────────────────────
        void Esc(std::string& out, const std::string& s) {
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

        bool ExtractStr(const std::string& s, const char* key, std::string& out) {
            std::string pat = static_cast<const char*>(AY_OBFUSCATE("\""));
            pat += key;
            pat += static_cast<const char*>(AY_OBFUSCATE("\":"));
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

        // 첫 Add() 호출 시 기존 파일이 있으면 메모리에 적재.
        void LoadExisting() {
            std::ifstream f(LogPath().c_str(), std::ios::binary);
            if (!f.is_open()) return;
            std::string s((std::istreambuf_iterator<char>(f)),
                           std::istreambuf_iterator<char>());

            size_t p = 0;
            while ((p = s.find(static_cast<const char*>(AY_OBFUSCATE("\"time\":")), p)) != std::string::npos) {
                size_t blockEnd = s.find('}', p);
                if (blockEnd == std::string::npos) break;
                std::string block = s.substr(p, blockEnd - p);

                Entry e;
                ExtractStr(block, AY_OBFUSCATE("time"), e.time);
                ExtractStr(block, AY_OBFUSCATE("msg"),  e.msg);

                if (!e.time.empty()) g_entries.push_back(std::move(e));
                p = blockEnd + 1;
            }
        }

        // 메모리 → log.json (tmp 파일에 쓰고 atomic move).
        void WriteFile() {
            std::string j = static_cast<const char*>(AY_OBFUSCATE("{\n  \"logs\": ["));
            for (size_t i = 0; i < g_entries.size(); ++i) {
                const Entry& e = g_entries[i];
                if (i) j += ',';
                j += static_cast<const char*>(AY_OBFUSCATE("\n    { \"time\": "));
                Esc(j, e.time);
                j += static_cast<const char*>(AY_OBFUSCATE(", \"msg\": "));
                Esc(j, e.msg);
                j += static_cast<const char*>(AY_OBFUSCATE(" }"));
            }
            j += static_cast<const char*>(AY_OBFUSCATE("\n  ]\n}\n"));

            std::wstring path = LogPath();
            std::wstring tmp  = path
                + static_cast<const wchar_t*>(AY_OBFUSCATE(L".tmp"));
            {
                std::ofstream f(tmp.c_str(), std::ios::binary | std::ios::trunc);
                if (!f.is_open()) return;
                f.write(j.data(), (std::streamsize)j.size());
            }
            MoveFileExW(tmp.c_str(), path.c_str(), MOVEFILE_REPLACE_EXISTING);
        }

    } // anonymous namespace

    // ─── 공개 API ─────────────────────────────────────────────────
    void SetNotifyWindow(HWND hWnd, UINT msgId)
    {
        g_notifyWnd = hWnd;
        g_notifyMsg = msgId;
    }

    void Add(const std::wstring& s)
    {
        Entry e;
        e.time = NowText();
        e.msg  = Utf8(s);

        {
            std::lock_guard<std::mutex> lk(g_mu);
            if (!g_loaded) {
                g_loaded = true;
                LoadExisting();
            }
            g_entries.push_back(std::move(e));
            if (g_entries.size() > 500) g_entries.erase(g_entries.begin());
            WriteFile();
        }
        if (g_notifyWnd && g_notifyMsg)
            PostMessage(g_notifyWnd, g_notifyMsg, 0, 0);
    }

    void Add(const wchar_t* s)
    {
        Add(std::wstring(s ? s : L""));
    }

    std::wstring GetAll()
    {
        std::wstring all;
        std::lock_guard<std::mutex> lk(g_mu);
        for (auto& e : g_entries) {
            all += L"[";
            all += Wide(e.time);
            all += L"] ";
            all += Wide(e.msg);
            all += L"\r\n";
        }
        return all;
    }

    // e.time 은 "YYYY-MM-DD HH:MM:SS" 고정 폭이라 lexicographic 비교가
    // 시간 순서와 동일. cutoff 문자열만 만들어 그보다 작은 엔트리 제거.
    void PruneByDays(int days)
    {
        if (days <= 0) return;  // 영구 보관

        std::time_t now = std::time(nullptr);
        std::time_t cutoffT = now - static_cast<std::time_t>(days) * 86400;
        std::tm tmCut{};
        localtime_s(&tmCut, &cutoffT);
        char cutoff[32];
        std::snprintf(cutoff, sizeof(cutoff),
            AY_OBFUSCATE("%04d-%02d-%02d %02d:%02d:%02d"),
            tmCut.tm_year + 1900, tmCut.tm_mon + 1, tmCut.tm_mday,
            tmCut.tm_hour, tmCut.tm_min, tmCut.tm_sec);

        std::lock_guard<std::mutex> lk(g_mu);
        if (!g_loaded) {
            g_loaded = true;
            LoadExisting();
        }
        auto it = std::remove_if(g_entries.begin(), g_entries.end(),
            [&](const Entry& e) { return e.time < cutoff; });
        if (it == g_entries.end()) return;
        g_entries.erase(it, g_entries.end());
        WriteFile();
    }

    // ─── Msg: 의미 함수들 ──────────────────────────────────────────
    // noinline 으로 호출처에 인라인되지 않게 강제 — 디크립트 코드/문자열은 logger.cpp 본문에만 존재.
    namespace Msg
    {
        // App lifecycle
        __declspec(noinline) void AppSettingsLoaded() {
            Add(AY_OBFUSCATE(L"[App] Settings loaded."));
        }
        __declspec(noinline) void AppPolicyDbLoaded() {
            Add(AY_OBFUSCATE(L"[App] Policy DB loaded from file."));
        }
        __declspec(noinline) void AppStart() {
            Add(AY_OBFUSCATE(L"[App] Start (Tray resident)"));
        }
        __declspec(noinline) void AppSkippedInitialScan() {
            Add(AY_OBFUSCATE(L"[App] Skipped initial scan (policy DB exists)."));
        }

        // Init
        __declspec(noinline) void InitHidRegistered(bool ok) {
            Add(ok
                ? std::wstring(AY_OBFUSCATE(L"[Init] HID notifications registered."))
                : std::wstring(AY_OBFUSCATE(L"[Init] HID notification registration failed.")));
        }

        // Hook
        __declspec(noinline) void HookInstalled() {
            Add(AY_OBFUSCATE(L"[Hook] Keyboard hook installed. All keyboard input blocked."));
        }
        __declspec(noinline) void HookInstallFailed() {
            Add(AY_OBFUSCATE(L"[Hook] Keyboard hook install FAILED."));
        }
        __declspec(noinline) void HookRemoved() {
            Add(AY_OBFUSCATE(L"[Hook] Keyboard hook removed. Keyboard restored."));
        }

        // Auto
        __declspec(noinline) void AutoArrivalDuplicate() {
            Add(AY_OBFUSCATE(L"[Auto] Duplicate arrival ignored (same DevicePath)."));
        }
        __declspec(noinline) void AutoArrival() {
            Add(AY_OBFUSCATE(L"[Auto] HID arrival detected."));
        }
        __declspec(noinline) void AutoRemoved() {
            Add(AY_OBFUSCATE(L"[Auto] HID removed."));
        }

        // Approval
        __declspec(noinline) void ApprovalAutoBlockCandidateVidPid(const std::wstring& name) {
            Add(std::wstring(AY_OBFUSCATE(L"[Approval] Auto-block candidate (blacklisted VID/PID): ")) + name);
        }
        __declspec(noinline) void ApprovalNoUnknownDevices() {
            Add(AY_OBFUSCATE(L"[Approval] No unknown HID devices."));
        }
        __declspec(noinline) void ApprovalCandidate(const std::wstring& name) {
            Add(std::wstring(AY_OBFUSCATE(L"[Approval] Candidate: ")) + name);
        }
        __declspec(noinline) void ApprovalAutoBlock(const std::wstring& name) {
            Add(std::wstring(AY_OBFUSCATE(L"[Approval] Auto-block (blacklisted): ")) + name);
        }
        __declspec(noinline) void ApprovalAutoAllow(const std::wstring& name) {
            Add(std::wstring(AY_OBFUSCATE(L"[Approval] Auto-allow (whitelisted): ")) + name);
        }
        __declspec(noinline) void ApprovalUnknownStartHook(const std::wstring& name) {
            Add(std::wstring(AY_OBFUSCATE(L"[Approval] Unknown device → starting hook + viewer: ")) + name);
        }

        // UI
        __declspec(noinline) void ShowingApprovalDialog() {
            Add(AY_OBFUSCATE(L"[UI] Showing approval list dialog..."));
        }
        __declspec(noinline) void Canceled() {
            Add(AY_OBFUSCATE(L"[UI] Canceled. No policy changes."));
        }
        __declspec(noinline) void Saved() {
            Add(AY_OBFUSCATE(L"[UI] Saved. Applying policy..."));
        }

        // State
        __declspec(noinline) void SavedAsWhitelist(unsigned long long fingerprint) {
            Add(std::wstring(AY_OBFUSCATE(L"[State] Saved as Whitelist: ")) + std::to_wstring(fingerprint));
        }
        __declspec(noinline) void SavedAsBlacklist(unsigned long long fingerprint) {
            Add(std::wstring(AY_OBFUSCATE(L"[State] Saved as Blacklist: ")) + std::to_wstring(fingerprint));
        }

        // Block
        // ─── [Block] prefix 의미 분류 ────────────────────────────────────────
        // device_blocker 모듈에서 발생하는 모든 devnode 조작 로그를 [Block] prefix
        // 로 통일해서 출력한다. 메시지 본문이 실제 동작을 구분:
        //
        //   🛑 진짜 차단:
        //     "Blacklisted HID disabled[: <id>]"  — devnode disable 성공
        //     "Blacklisted HID ejected: <id>"    — eject 성공 (disable 실패 fallback)
        //     "Failed to disable/eject ..."      — 두 방식 모두 실패
        //     "Result=<n>, ..."                  — auto-block 흐름의 결과 요약
        //     "Saved blacklist item block result=..."  — blacklist 저장 직후 차단 결과
        //
        //   ✅ 활성화 (차단 아님):
        //     "Re-enabled device: <id>"          — devnode enable 성공
        //     "Saved whitelist item enable result=..."  — whitelist 저장 직후 활성화 결과
        //
        //   🔄 phantom devnode 정리 (차단 아님):
        //     "Removed disabled device for rescan: <id>"
        //       — 시스템에 남은 유령 devnode (이전 연결 흔적). 다음 PnP rescan 에서
        //         자동 재인식되도록 정리. 같은 VID/PID 가 여러 포트에 꽂혔다 빠진
        //         흔적이 쌓이면 다수 출력될 수 있음 (정상).
        //
        // 분석 시 메시지 본문 키워드("disabled" / "Re-enabled" / "Removed ...")
        // 로 구분할 것. [Block] prefix 만 보고 차단 여부를 판단하지 말 것.
        // ──────────────────────────────────────────────────────────────────
        __declspec(noinline) void BlockSavedWhitelistEnableResult(int result, const std::wstring& detail) {
            Add(std::wstring(AY_OBFUSCATE(L"[Block] Saved whitelist item enable result=")) + std::to_wstring(result) + std::wstring(AY_OBFUSCATE(L", ")) + detail);
        }
        __declspec(noinline) void BlockSavedBlacklistResult(int result, const std::wstring& detail) {
            Add(std::wstring(AY_OBFUSCATE(L"[Block] Saved blacklist item block result=")) + std::to_wstring(result) + std::wstring(AY_OBFUSCATE(L", ")) + detail);
        }
        __declspec(noinline) void BlockResult(int result, const std::wstring& detail) {
            Add(std::wstring(AY_OBFUSCATE(L"[Block] Result=")) + std::to_wstring(result) + std::wstring(AY_OBFUSCATE(L", ")) + detail);
        }
        // 🛑 진짜 차단 — devnode disable 성공
        __declspec(noinline) void BlockDisabledWithId(const std::wstring& instanceId) {
            Add(std::wstring(AY_OBFUSCATE(L"[Block] Blacklisted HID disabled: ")) + instanceId);
        }
        __declspec(noinline) void BlockDisabled() {
            Add(AY_OBFUSCATE(L"[Block] Blacklisted HID disabled."));
        }
        // 🛑 진짜 차단 — eject 성공 (disable 실패 시 fallback)
        __declspec(noinline) void BlockEjectedWithId(const std::wstring& instanceId) {
            Add(std::wstring(AY_OBFUSCATE(L"[Block] Blacklisted HID ejected: ")) + instanceId);
        }
        // ❌ 차단 실패 — disable + eject 모두 실패
        __declspec(noinline) void BlockFailed() {
            Add(AY_OBFUSCATE(L"[Block] Failed to disable/eject blacklisted HID."));
        }
        // ✅ 활성화 — 화이트리스트 추가 후 devnode 재활성화 성공 (차단 아님)
        __declspec(noinline) void BlockReEnabled(const std::wstring& instanceId) {
            Add(std::wstring(AY_OBFUSCATE(L"[Block] Re-enabled device: ")) + instanceId);
        }
        // 🔄 phantom devnode 정리 — 차단 아님. 다음 PnP rescan 에서 재인식됨.
        __declspec(noinline) void BlockRemovedForRescan(const std::wstring& instanceId) {
            Add(std::wstring(AY_OBFUSCATE(L"[Block] Removed disabled device for rescan: ")) + instanceId);
        }

        // PolicyUI
        __declspec(noinline) void PolicyNoSelection() {
            Add(AY_OBFUSCATE(L"[Policy] No policy item selected."));
        }
        __declspec(noinline) void PolicyAllowedSelected(const std::wstring& name) {
            Add(std::wstring(AY_OBFUSCATE(L"[Policy] Allowed selected item: ")) + name);
        }
        __declspec(noinline) void PolicyEnableResult(int result, const std::wstring& detail) {
            Add(std::wstring(AY_OBFUSCATE(L"[Policy] Enable result=")) + std::to_wstring(result) + std::wstring(AY_OBFUSCATE(L", ")) + detail);
        }

        // SettingsUI
        __declspec(noinline) void SettingsSaveResult(bool ok) {
            Add(ok
                ? std::wstring(AY_OBFUSCATE(L"[Settings] Saved settings to disk."))
                : std::wstring(AY_OBFUSCATE(L"[Settings] Save failed.")));
        }
        __declspec(noinline) void SettingsReloadResult(bool ok) {
            Add(ok
                ? std::wstring(AY_OBFUSCATE(L"[Settings] Reloaded settings from disk."))
                : std::wstring(AY_OBFUSCATE(L"[Settings] Reload failed.")));
        }

        // Score
        __declspec(noinline) void ScoreFinal(double final_score, int level) {
            const wchar_t* lvName;
            switch (level) {
                case 0: lvName = AY_OBFUSCATE(L"SAFE");    break;
                case 1: lvName = AY_OBFUSCATE(L"NORMAL");  break;
                case 2: lvName = AY_OBFUSCATE(L"WARN");    break;
                case 3: lvName = AY_OBFUSCATE(L"DANGER");  break;
                default: lvName = AY_OBFUSCATE(L"UNKNOWN"); break;
            }
            wchar_t buf[128];
            _snwprintf_s(buf, _TRUNCATE, AY_OBFUSCATE(L"[Score] final=%.2f, %ls"), final_score, lvName);
            Add(buf);
        }
    }
}
