#include "ui_compound_scorer.h"
#include "keyboard_hook.h"
#include "key_decoder.h"
#include "shell_detector.h"
#include "threat_matcher.h"
#include "compound_scorer.h"
#include "hotkey_scorer.h"
#include <string>

// ──────────────────────────────────────────────────────────────────────
// CompoundScorerUI 구현
//   - 한 창에 두 영역:
//     [상단] CompoundScorer Verdict — 최종 점수 + 4단계 분류 + 모듈별 점수
//     [하단] ThreatMatcher 상세 로그 — 명령어/토큰별 매칭 + 세션 합산
//   - [Refresh] 버튼: 현재 키 버퍼로 다시 분석
//   - [Close] 버튼: 창 닫기
// ──────────────────────────────────────────────────────────────────────

namespace {

    const wchar_t* CLS_NAME = L"BadUSB_CompoundScorer";
    HWND  g_hWnd     = nullptr;
    HWND  g_hVerdict = nullptr;
    HWND  g_hDetail  = nullptr;

    const UINT_PTR IDC_VERDICT  = 3001;
    const UINT_PTR IDC_DETAIL   = 3002;
    const UINT_PTR IDC_REFRESH  = 3003;
    const UINT_PTR IDC_CLOSE    = 3004;

    // 한 번의 분석 사이클: 버퍼 → Decode → ShellDetector → ThreatMatcher → CompoundScorer
    // 결과를 두 영역 (verdict / detail) 에 채움.
    void RunAnalysisAndRender()
    {
        if (!g_hVerdict || !g_hDetail) return;

        auto keys    = KeyboardHook::GetBufferedKeys();
        auto decoded = KeyDecoder::Decode(keys);
        auto shell   = ShellDetector::Detect(decoded);
        auto tm      = ThreatMatcher::Analyze(decoded, shell.shell);
        auto hk      = HotkeyScorer::Analyze(decoded.hotkeys);

        // URL / Typing 모듈은 미구현 — 0점 전달
        // double url    = UrlClassifier::Score(decoded);   // 미구현
        // double typing = TypingAnalyzer::Score(keys);     // 미구현
        auto verdict = CompoundScorer::Combine(
            tm.session_score,
            /*url   */ 0.0,
            /*typing*/ 0.0,
            hk.final_score
        );

        SetWindowTextW(g_hVerdict, CompoundScorer::FormatVerdict(verdict).c_str());
        // 상세 영역에 ThreatMatcher + HotkeyScorer 로그 모두 표시
        std::wstring detail = ThreatMatcher::FormatLog(tm)
                            + L"\r\n" + HotkeyScorer::FormatLog(hk);
        SetWindowTextW(g_hDetail, detail.c_str());
    }

    LRESULT CALLBACK ScorerProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam)
    {
        switch (msg) {
        case WM_COMMAND:
            if (LOWORD(wParam) == IDC_REFRESH) {
                RunAnalysisAndRender();
                return 0;
            }
            if (LOWORD(wParam) == IDC_CLOSE) {
                DestroyWindow(hWnd);
                return 0;
            }
            break;

        case WM_CLOSE:
            DestroyWindow(hWnd);
            return 0;

        case WM_DESTROY:
            g_hWnd     = nullptr;
            g_hVerdict = nullptr;
            g_hDetail  = nullptr;
            return 0;
        }
        return DefWindowProcW(hWnd, msg, wParam, lParam);
    }

    void RegisterClassOnce(HINSTANCE hInst)
    {
        static bool done = false;
        if (done) return;

        WNDCLASSEXW wc{};
        wc.cbSize        = sizeof(wc);
        wc.lpfnWndProc   = ScorerProc;
        wc.hInstance     = hInst;
        wc.hCursor       = LoadCursor(nullptr, IDC_ARROW);
        wc.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
        wc.lpszClassName = CLS_NAME;
        RegisterClassExW(&wc);
        done = true;
    }

} // anonymous namespace


namespace CompoundScorerUI
{
    void Show(HWND hParent)
    {
        // 이미 열려 있으면 → 최상위로 올리고 재분석
        if (g_hWnd) {
            SetForegroundWindow(g_hWnd);
            RunAnalysisAndRender();
            return;
        }

        HINSTANCE hInst = GetModuleHandle(nullptr);
        RegisterClassOnce(hInst);

        const int W = 720, H = 600;
        int sx = GetSystemMetrics(SM_CXSCREEN);
        int sy = GetSystemMetrics(SM_CYSCREEN);

        g_hWnd = CreateWindowExW(
            WS_EX_TOPMOST,
            CLS_NAME, L"BadUSB Guard - Compound Score",
            WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU,
            (sx - W) / 2, (sy - H) / 2, W, H,
            hParent, nullptr, hInst, nullptr
        );
        if (!g_hWnd) return;

        HFONT hMono = CreateFontW(15, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
            DEFAULT_CHARSET, 0, 0, 0, 0, L"Consolas");

        // ─── 상단 라벨 + Verdict 영역 ────────────────────────────
        CreateWindowExW(0, L"STATIC",
            L"Final Score (CompoundScorer Verdict)",
            WS_CHILD | WS_VISIBLE,
            15, 10, 600, 18, g_hWnd, nullptr, hInst, nullptr);

        g_hVerdict = CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT", L"",
            WS_CHILD | WS_VISIBLE | WS_VSCROLL |
            ES_MULTILINE | ES_READONLY | ES_AUTOVSCROLL,
            15, 30, 685, 175, g_hWnd, (HMENU)IDC_VERDICT, hInst, nullptr);
        if (hMono) SendMessageW(g_hVerdict, WM_SETFONT, (WPARAM)hMono, TRUE);

        // ─── 하단 라벨 + ThreatMatcher 상세 영역 ─────────────────
        CreateWindowExW(0, L"STATIC",
            L"ThreatMatcher Detail",
            WS_CHILD | WS_VISIBLE,
            15, 215, 600, 18, g_hWnd, nullptr, hInst, nullptr);

        // ES_AUTOHSCROLL/WS_HSCROLL 제거 → 자동 줄바꿈 (word wrap) 활성화
        g_hDetail = CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT", L"",
            WS_CHILD | WS_VISIBLE | WS_VSCROLL |
            ES_MULTILINE | ES_READONLY | ES_AUTOVSCROLL,
            15, 235, 685, 280, g_hWnd, (HMENU)IDC_DETAIL, hInst, nullptr);
        if (hMono) SendMessageW(g_hDetail, WM_SETFONT, (WPARAM)hMono, TRUE);

        // ─── 버튼 ──────────────────────────────────────────────
        CreateWindowExW(0, L"BUTTON", L"Refresh",
            WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
            450, 525, 110, 30, g_hWnd, (HMENU)IDC_REFRESH, hInst, nullptr);

        CreateWindowExW(0, L"BUTTON", L"Close",
            WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
            580, 525, 110, 30, g_hWnd, (HMENU)IDC_CLOSE, hInst, nullptr);

        // 첫 분석 즉시 실행 + 표시
        RunAnalysisAndRender();

        ShowWindow(g_hWnd, SW_SHOW);
        UpdateWindow(g_hWnd);
    }

    void Close()
    {
        if (g_hWnd) {
            DestroyWindow(g_hWnd);
            g_hWnd = nullptr;
        }
    }
}
