#include "shell_detector.h"
#include "keyboard_shortcuts_ref.h"

namespace ShellDetector
{
    // ----- 셸 토큰 매칭 -----
    static bool IsCmdToken(const std::wstring& s) {
        return s == L"cmd" || s == L"cmd.exe";
    }
    static bool IsPsToken(const std::wstring& s) {
        return s == L"powershell" || s == L"powershell.exe" ||
               s == L"pwsh"       || s == L"pwsh.exe";
    }

    // ----- Win+X 서브메뉴 키 → 셸 종류 매핑 -----
    // ShellDetector 자체 지식: 어느 메뉴 키가 어느 셸인지만 정의.
    // isAdmin 정보는 KeyShortcutRef::WIN_X_MENU 단일 출처 사용.
    struct WinXShellKey {
        wchar_t key;
        Shell   shell;
    };

    static constexpr WinXShellKey WINX_SHELL_KEYS[] = {
        { L'A', SHELL_POWERSHELL },   // 터미널 (관리자)
        { L'I', SHELL_POWERSHELL },   // Windows PowerShell (구버전)
        { L'J', SHELL_POWERSHELL },   // 터미널
    };

    // 메뉴 키 → 셸 종류 + (KeyShortcutRef에서 isAdmin 조회)
    static bool LookupWinXShell(wchar_t key, Shell& outShell, bool& outElevated)
    {
        for (const auto& m : WINX_SHELL_KEYS) {
            if (m.key != key) continue;
            outShell = m.shell;
            // isAdmin은 KeyShortcutRef를 단일 출처로
            auto entry = KeyShortcutRef::LookupWinXMenu(key);
            outElevated = entry ? entry->isAdmin : false;
            return true;
        }
        return false;
    }

    // ----- 핫키 콤보 검사 헬퍼 -----
    static bool IsRunDialogHotkey(const KeyDecoder::HotkeyEvent& h)
    {
        // Win+R 단독, 또는 Win+X → R (실행 창)
        if (h.combo == L"Win+R") return true;
        if (h.combo == L"Win+X" && h.submenuKey == L'R') return true;
        return false;
    }

    Result Detect(const KeyDecoder::DecodeResult& decoded)
    {
        Result r{ SHELL_NONE, 0, false };

        // 1) Win+X 서브메뉴가 직접 셸을 여는 경우 (가장 명확한 신호)
        for (const auto& h : decoded.hotkeys) {
            if (h.combo != L"Win+X" || h.submenuKey == 0) continue;
            Shell s; bool elev;
            if (LookupWinXShell(h.submenuKey, s, elev)) {
                return { s, h.lineIndex, elev };
            }
        }

        // 2) Win+R (또는 Win+X → R) 직후 같은 줄에 셸 명령어
        for (const auto& h : decoded.hotkeys) {
            if (!IsRunDialogHotkey(h)) continue;

            for (const auto& cmd : decoded.commands) {
                if (cmd.lineIndex != h.lineIndex) continue;
                if (IsCmdToken(cmd.command))
                    return { SHELL_CMD, h.lineIndex, false };
                if (IsPsToken(cmd.command))
                    return { SHELL_POWERSHELL, h.lineIndex, false };
                // 첫 명령어가 셸 토큰 아니면 더 안 봄 (notepad 등)
                break;
            }
        }

        // 3) 명령어 첫 토큰 직접 매칭 (가장 먼저 등장한 셸)
        for (const auto& cmd : decoded.commands) {
            if (IsCmdToken(cmd.command))
                return { SHELL_CMD, cmd.lineIndex, false };
            if (IsPsToken(cmd.command))
                return { SHELL_POWERSHELL, cmd.lineIndex, false };
        }

        // 4) 못 찾음
        return r;
    }
}
