#include "ui_buffer_view.h"
#include "keyboard_hook.h"
#include "key_decoder.h"
#include "keyboard_shortcuts_ref.h"
#include "shell_detector.h"
#include <string>

// --- 가상 키코드 → 문자열 변환 ---

static std::wstring VkCodeToString(DWORD vk)
{
    // 알파벳 A-Z
    if (vk >= 0x41 && vk <= 0x5A)
        return std::wstring(1, (wchar_t)vk);

    // 숫자 0-9
    if (vk >= 0x30 && vk <= 0x39)
        return std::wstring(1, (wchar_t)vk);

    // 넘패드 0-9
    if (vk >= VK_NUMPAD0 && vk <= VK_NUMPAD9) {
        wchar_t buf[8];
        wsprintfW(buf, L"Num%d", vk - VK_NUMPAD0);
        return buf;
    }

    // F1-F12
    if (vk >= VK_F1 && vk <= VK_F12) {
        wchar_t buf[8];
        wsprintfW(buf, L"F%d", vk - VK_F1 + 1);
        return buf;
    }

    switch (vk) {
    case VK_RETURN:     return L"Enter";
    case VK_SPACE:      return L"Space";
    case VK_BACK:       return L"Backspace";
    case VK_TAB:        return L"Tab";
    case VK_ESCAPE:     return L"Esc";
    case VK_LSHIFT:
    case VK_RSHIFT:
    case VK_SHIFT:      return L"Shift";
    case VK_LCONTROL:
    case VK_RCONTROL:
    case VK_CONTROL:    return L"Ctrl";
    case VK_LMENU:
    case VK_RMENU:
    case VK_MENU:       return L"Alt";
    case VK_LWIN:
    case VK_RWIN:       return L"Win";
    case VK_CAPITAL:    return L"CapsLock";
    case VK_DELETE:     return L"Del";
    case VK_INSERT:     return L"Ins";
    case VK_HOME:       return L"Home";
    case VK_END:        return L"End";
    case VK_PRIOR:      return L"PgUp";
    case VK_NEXT:       return L"PgDn";
    case VK_LEFT:       return L"Left";
    case VK_RIGHT:      return L"Right";
    case VK_UP:         return L"Up";
    case VK_DOWN:       return L"Down";
    case VK_OEM_PERIOD: return L".";
    case VK_OEM_COMMA:  return L",";
    case VK_OEM_MINUS:  return L"-";
    case VK_OEM_PLUS:   return L"=";
    case VK_OEM_1:      return L";";
    case VK_OEM_2:      return L"/";
    case VK_OEM_3:      return L"`";
    case VK_OEM_4:      return L"[";
    case VK_OEM_5:      return L"\\";
    case VK_OEM_6:      return L"]";
    case VK_OEM_7:      return L"'";
    default: {
        wchar_t buf[8];
        wsprintfW(buf, L"0x%02X", vk);
        return buf;
    }
    }
}

// --- 키 기록 → 표시용 텍스트 생성 ---

static std::wstring BuildKeysText(const std::vector<KeyboardHook::KeyRecord>& keys)
{
    if (keys.empty())
        return L"(No keystrokes detected)";

    std::wstring result;
    int count = 0;

    for (const auto& k : keys) {
        // key-down만 표시 (key-up 무시)
        if (k.flags & 0x80)  // LLKHF_UP == 0x80
            continue;

        if (count > 0)
            result += L" ";

        result += L"[" + VkCodeToString(k.vkCode) + L"]";
        count++;
    }

    if (count == 0)
        return L"(No keystrokes detected)";

    return result;
}

// --- 버퍼 뷰어 창 ---

static const wchar_t* VIEWER_CLASS = L"BadUSB_BufferViewer";
static bool g_result = false;
static bool g_closed = false;
static HWND g_hEdit = nullptr;
static HWND g_hModelessWnd = nullptr;

static const UINT_PTR IDC_EDIT_KEYS       = 2001;
static const UINT_PTR IDC_BTN_ALLOW       = 2002;
static const UINT_PTR IDC_BTN_DENY        = 2003;
static const UINT_PTR IDC_LABEL_DEVICE    = 2004;
static const UINT_PTR IDC_LABEL_TITLE     = 2005;
static const UINT_PTR IDC_TIMER_REFRESH   = 2006;
static const UINT_PTR IDC_BTN_COPY        = 2007;
static const UINT_PTR IDC_BTN_NORMALIZE   = 2008;

// --- 정규화 디버그 창 전용 ---
static const wchar_t* NORMDEBUG_CLASS     = L"BadUSB_NormDebug";
static HWND           g_hNormDebugWnd     = nullptr;
static const UINT_PTR IDC_NORMDEBUG_CLOSE = 2100;
static const UINT_PTR IDC_NORMDEBUG_RAW   = 2101;
static const UINT_PTR IDC_NORMDEBUG_NORM  = 2102;

static LRESULT CALLBACK ViewerProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    switch (msg) {
    case WM_COMMAND:
        if (LOWORD(wParam) == IDC_BTN_ALLOW) {
            g_result = true;
            g_closed = true;
            DestroyWindow(hWnd);
            return 0;
        }
        if (LOWORD(wParam) == IDC_BTN_DENY) {
            g_result = false;
            g_closed = true;
            DestroyWindow(hWnd);
            return 0;
        }
        if (LOWORD(wParam) == IDC_BTN_COPY && g_hEdit) {
            int len = GetWindowTextLengthW(g_hEdit);
            if (len > 0 && OpenClipboard(hWnd)) {
                HGLOBAL hMem = GlobalAlloc(GMEM_MOVEABLE, (len + 1) * sizeof(wchar_t));
                if (hMem) {
                    wchar_t* p = static_cast<wchar_t*>(GlobalLock(hMem));
                    GetWindowTextW(g_hEdit, p, len + 1);
                    GlobalUnlock(hMem);
                    EmptyClipboard();
                    SetClipboardData(CF_UNICODETEXT, hMem);
                }
                CloseClipboard();
            }
            return 0;
        }
        if (LOWORD(wParam) == IDC_BTN_NORMALIZE) {
            BufferViewUI::OpenNormalizeDebug();
            return 0;
        }
        break;

    case WM_TIMER:
        if (wParam == IDC_TIMER_REFRESH && g_hEdit) {
            SCROLLINFO si{};
            si.cbSize = sizeof(si);
            si.fMask = SIF_ALL;
            GetScrollInfo(g_hEdit, SB_VERT, &si);
            bool wasAtBottom = (si.nPos + (int)si.nPage >= si.nMax) || (si.nMax == 0);

            // SetWindowTextW 호출 전 첫 번째 보이는 줄 저장
            // (SetWindowTextW는 Edit 컨트롤 스크롤을 항상 맨 위로 리셋함)
            int firstVisible = (int)SendMessageW(g_hEdit, EM_GETFIRSTVISIBLELINE, 0, 0);

            auto keys = KeyboardHook::GetBufferedKeys();
            std::wstring text = BuildKeysText(keys);
            SetWindowTextW(g_hEdit, text.c_str());

            if (wasAtBottom) {
                SendMessageW(g_hEdit, EM_SETSEL, (WPARAM)text.size(), (LPARAM)text.size());
                SendMessageW(g_hEdit, EM_SCROLLCARET, 0, 0);
            } else {
                // 사용자가 스크롤을 올렸으면 기존 위치 복원
                SendMessageW(g_hEdit, EM_LINESCROLL, 0, firstVisible);
            }
        }
        return 0;

    case WM_CLOSE:
        // 모달리스 모니터링 창은 X 버튼으로 닫기 허용
        if (hWnd == g_hModelessWnd) {
            DestroyWindow(hWnd);
            return 0;
        }
        // 모달 뷰어는 Deny 처리
        g_result = false;
        g_closed = true;
        DestroyWindow(hWnd);
        return 0;

    case WM_DESTROY:
        KillTimer(hWnd, IDC_TIMER_REFRESH);
        g_hEdit = nullptr;
        if (hWnd == g_hModelessWnd)
            g_hModelessWnd = nullptr;
        return 0;
    }

    return DefWindowProcW(hWnd, msg, wParam, lParam);
}

static void RegisterViewerClass(HINSTANCE hInst)
{
    static bool registered = false;
    if (registered) return;

    WNDCLASSEXW wc{};
    wc.cbSize = sizeof(wc);
    wc.lpfnWndProc = ViewerProc;
    wc.hInstance = hInst;
    wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
    wc.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
    wc.lpszClassName = VIEWER_CLASS;
    RegisterClassExW(&wc);
    registered = true;
}

// --- 정규화 디버그 창 ---

static LRESULT CALLBACK NormDebugProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    switch (msg) {
    case WM_COMMAND:
        if (LOWORD(wParam) == IDC_NORMDEBUG_CLOSE) {
            DestroyWindow(hWnd);
            return 0;
        }
        break;
    case WM_CLOSE:
        DestroyWindow(hWnd);
        return 0;
    case WM_DESTROY:
        g_hNormDebugWnd = nullptr;
        return 0;
    }
    return DefWindowProcW(hWnd, msg, wParam, lParam);
}

static void RegisterNormDebugClass(HINSTANCE hInst)
{
    static bool registered = false;
    if (registered) return;

    WNDCLASSEXW wc{};
    wc.cbSize = sizeof(wc);
    wc.lpfnWndProc = NormDebugProc;
    wc.hInstance = hInst;
    wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
    wc.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
    wc.lpszClassName = NORMDEBUG_CLASS;
    RegisterClassExW(&wc);
    registered = true;
}

// 장치 라벨 텍스트 생성
static void FormatDeviceLabel(wchar_t* buf, size_t bufLen,
                              uint16_t vid, uint16_t pid,
                              const std::wstring& deviceName)
{
    if (deviceName.empty())
        wsprintfW(buf, L"Device:  VID=0x%04X   PID=0x%04X", vid, pid);
    else
        wsprintfW(buf, L"Device:  %s  (VID=0x%04X  PID=0x%04X)",
                  deviceName.c_str(), vid, pid);
}

// --- 모달 뷰어 (Allow/Deny 버튼 포함, 차단) ---

namespace BufferViewUI
{
    bool Show(HWND hParent, uint16_t vid, uint16_t pid,
              const std::wstring& deviceName)
    {
        HINSTANCE hInst = (HINSTANCE)GetWindowLongPtrW(hParent, GWLP_HINSTANCE);
        RegisterViewerClass(hInst);

        g_result = false;
        g_closed = false;

        const int W = 500, H = 350;

        int sx = GetSystemMetrics(SM_CXSCREEN);
        int sy = GetSystemMetrics(SM_CYSCREEN);
        int x = (sx - W) / 2;
        int y = (sy - H) / 2;

        HWND hWnd = CreateWindowExW(
            WS_EX_TOPMOST,
            VIEWER_CLASS, L"BadUSB Guard - Buffer Viewer",
            WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU,
            x, y, W, H,
            hParent, nullptr, hInst, nullptr
        );

        if (!hWnd) return false;

        // 장치 정보 라벨
        {
            wchar_t buf[256];
            FormatDeviceLabel(buf, 256, vid, pid, deviceName);
            CreateWindowExW(0, L"STATIC", buf,
                WS_CHILD | WS_VISIBLE,
                20, 15, 450, 20, hWnd, (HMENU)IDC_LABEL_DEVICE, hInst, nullptr);
        }

        CreateWindowExW(0, L"STATIC", L"Captured Keystrokes:",
            WS_CHILD | WS_VISIBLE,
            20, 45, 200, 20, hWnd, (HMENU)IDC_LABEL_TITLE, hInst, nullptr);

        {
            g_hEdit = CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT",
                L"(No keystrokes detected)",
                WS_CHILD | WS_VISIBLE | WS_VSCROLL |
                ES_MULTILINE | ES_READONLY | ES_AUTOVSCROLL,
                20, 70, 445, 170, hWnd, (HMENU)IDC_EDIT_KEYS, hInst, nullptr);

            HFONT hFont = CreateFontW(16, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
                DEFAULT_CHARSET, 0, 0, 0, 0, L"Consolas");
            if (hFont)
                SendMessageW(g_hEdit, WM_SETFONT, (WPARAM)hFont, TRUE);
        }

        SetTimer(hWnd, IDC_TIMER_REFRESH, 200, nullptr);

        CreateWindowExW(0, L"BUTTON", L"Copy",
            WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
            15, 260, 75, 35, hWnd, (HMENU)IDC_BTN_COPY, hInst, nullptr);

        CreateWindowExW(0, L"BUTTON", L"Normalize",
            WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
            100, 260, 90, 35, hWnd, (HMENU)IDC_BTN_NORMALIZE, hInst, nullptr);

        CreateWindowExW(0, L"BUTTON", L"Allow",
            WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
            205, 260, 90, 35, hWnd, (HMENU)IDC_BTN_ALLOW, hInst, nullptr);

        CreateWindowExW(0, L"BUTTON", L"Deny",
            WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
            305, 260, 90, 35, hWnd, (HMENU)IDC_BTN_DENY, hInst, nullptr);

        EnableWindow(hParent, FALSE);

        ShowWindow(hWnd, SW_SHOW);
        UpdateWindow(hWnd);

        MSG msg;
        while (!g_closed && GetMessageW(&msg, nullptr, 0, 0)) {
            TranslateMessage(&msg);
            DispatchMessageW(&msg);
        }

        EnableWindow(hParent, TRUE);
        SetForegroundWindow(hParent);

        return g_result;
    }

    // --- 모달리스 모니터링 창 (버튼 없음, 비차단) ---

    void Open(HWND hParent, uint16_t vid, uint16_t pid,
              const std::wstring& deviceName)
    {
        // 이미 열려 있으면 닫기
        if (g_hModelessWnd) {
            DestroyWindow(g_hModelessWnd);
            g_hModelessWnd = nullptr;
            g_hEdit = nullptr;
        }

        HINSTANCE hInst = (HINSTANCE)GetWindowLongPtrW(hParent, GWLP_HINSTANCE);
        RegisterViewerClass(hInst);

        const int W = 480, H = 280;

        // 화면 오른쪽 상단 배치 (승인 다이얼로그와 겹치지 않게)
        int sx = GetSystemMetrics(SM_CXSCREEN);
        int x = sx - W - 40;
        int y = 40;

        HWND hWnd = CreateWindowExW(
            WS_EX_TOPMOST,
            VIEWER_CLASS, L"BadUSB Guard - Keystroke Monitor",
            WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU,
            x, y, W, H,
            nullptr, nullptr, hInst, nullptr  // 부모 없음 → 독립 창
        );

        if (!hWnd) return;
        g_hModelessWnd = hWnd;

        // 장치 정보 라벨
        {
            wchar_t buf[256];
            FormatDeviceLabel(buf, 256, vid, pid, deviceName);
            CreateWindowExW(0, L"STATIC", buf,
                WS_CHILD | WS_VISIBLE,
                20, 15, 430, 20, hWnd, (HMENU)IDC_LABEL_DEVICE, hInst, nullptr);
        }

        CreateWindowExW(0, L"STATIC", L"Captured Keystrokes:",
            WS_CHILD | WS_VISIBLE,
            20, 45, 200, 20, hWnd, (HMENU)IDC_LABEL_TITLE, hInst, nullptr);

        {
            g_hEdit = CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT",
                L"(No keystrokes detected)",
                WS_CHILD | WS_VISIBLE | WS_VSCROLL |
                ES_MULTILINE | ES_READONLY | ES_AUTOVSCROLL,
                20, 70, 425, 130, hWnd, (HMENU)IDC_EDIT_KEYS, hInst, nullptr);

            HFONT hFont = CreateFontW(16, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
                DEFAULT_CHARSET, 0, 0, 0, 0, L"Consolas");
            if (hFont)
                SendMessageW(g_hEdit, WM_SETFONT, (WPARAM)hFont, TRUE);
        }

        CreateWindowExW(0, L"BUTTON", L"Copy",
            WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
            75, 215, 125, 30, hWnd, (HMENU)IDC_BTN_COPY, hInst, nullptr);

        CreateWindowExW(0, L"BUTTON", L"Normalize",
            WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
            255, 215, 145, 30, hWnd, (HMENU)IDC_BTN_NORMALIZE, hInst, nullptr);

        SetTimer(hWnd, IDC_TIMER_REFRESH, 200, nullptr);

        ShowWindow(hWnd, SW_SHOW);
        UpdateWindow(hWnd);
    }

    void Close()
    {
        if (g_hModelessWnd) {
            DestroyWindow(g_hModelessWnd);
            g_hModelessWnd = nullptr;
        }
        g_hEdit = nullptr;
    }

    // --- 정규화 디버그 창 ---
    // 현재 버퍼 스냅샷의 raw 표시 + 정규화 결과 예정 영역을 보여주는 디버그 창.
    // KeyDecoder::Decode() 구현 전까지 정규화 섹션은 "(미구현)" 플레이스홀더.

    void OpenNormalizeDebug()
    {
        // 이미 열려 있으면 최상위로 올리기만
        if (g_hNormDebugWnd) {
            SetForegroundWindow(g_hNormDebugWnd);
            return;
        }

        HINSTANCE hInst = GetModuleHandle(nullptr);
        RegisterNormDebugClass(hInst);

        const int W = 680, H = 510;
        int sx = GetSystemMetrics(SM_CXSCREEN);
        int sy = GetSystemMetrics(SM_CYSCREEN);

        HWND hWnd = CreateWindowExW(
            WS_EX_TOPMOST,
            NORMDEBUG_CLASS, L"BadUSB Guard - Normalize Debug",
            WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU,
            (sx - W) / 2, (sy - H) / 2, W, H,
            nullptr, nullptr, hInst, nullptr
        );
        if (!hWnd) return;
        g_hNormDebugWnd = hWnd;

        HFONT hMono = CreateFontW(15, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
            DEFAULT_CHARSET, 0, 0, 0, 0, L"Consolas");

        // --- Raw Buffer section ---
        CreateWindowExW(0, L"STATIC", L"Raw Buffer  (VkCode display / snapshot on button click)",
            WS_CHILD | WS_VISIBLE,
            15, 10, 645, 18, hWnd, nullptr, hInst, nullptr);

        HWND hRaw = CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT", L"",
            WS_CHILD | WS_VISIBLE | WS_VSCROLL | WS_HSCROLL |
            ES_MULTILINE | ES_READONLY | ES_AUTOHSCROLL,
            15, 30, 645, 165, hWnd, (HMENU)IDC_NORMDEBUG_RAW, hInst, nullptr);
        if (hMono) SendMessageW(hRaw, WM_SETFONT, (WPARAM)hMono, TRUE);

        {
            auto keys = KeyboardHook::GetBufferedKeys();
            std::wstring rawText = BuildKeysText(keys);
            SetWindowTextW(hRaw, rawText.c_str());
        }

        // --- Normalized section ---
        CreateWindowExW(0, L"STATIC",
            L"Normalized Result  (KeyDecoder::Decode)",
            WS_CHILD | WS_VISIBLE,
            15, 205, 645, 18, hWnd, nullptr, hInst, nullptr);

        HWND hNorm = CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT", L"",
            WS_CHILD | WS_VISIBLE | WS_VSCROLL |
            ES_MULTILINE | ES_READONLY | ES_AUTOVSCROLL,
            15, 225, 645, 200, hWnd, (HMENU)IDC_NORMDEBUG_NORM, hInst, nullptr);
        if (hMono) SendMessageW(hNorm, WM_SETFONT, (WPARAM)hMono, TRUE);

        // Decode() 호출 후 결과 표시
        {
            auto keys   = KeyboardHook::GetBufferedKeys();
            auto decoded = KeyDecoder::Decode(keys);

            std::wstring normText;

            // 줄 목록
            normText += L"=== Lines ===\r\n";
            for (size_t i = 0; i < decoded.lines.size(); ++i) {
                wchar_t prefix[32];
                wsprintfW(prefix, L"[%d] ", (int)i);
                normText += prefix;
                normText += decoded.lines[i].text;
                normText += L"\r\n";
            }

            // 단축키 이벤트
            normText += L"\r\n=== Hotkeys ===\r\n";
            if (decoded.hotkeys.empty()) {
                normText += L"(없음)\r\n";
            } else {
                for (auto& h : decoded.hotkeys) {
                    // 기본 표시: combo
                    normText += h.combo;
                    // Win+X 서브메뉴면 action 설명 추가 표시
                    if (h.submenuKey != 0) {
                        auto entry = KeyShortcutRef::LookupWinXMenu(h.submenuKey);
                        if (entry) {
                            normText += L" → ";
                            normText += entry->action;
                        }
                    }
                    wchar_t buf[32];
                    wsprintfW(buf, L"  @ line[%d]\r\n", (int)h.lineIndex);
                    normText += buf;
                }
            }

            // URL 목록
            normText += L"\r\n=== URLs ===\r\n";
            bool hasUrl = false;
            for (auto& line : decoded.lines) {
                for (auto& url : line.urls) {
                    hasUrl = true;
                    wchar_t buf[32];
                    wsprintfW(buf, L"[line %d] ", (int)url.lineIndex);
                    normText += buf;
                    // raw != resolved 이면 인코딩 있었음을 표시
                    if (url.raw != url.resolved) {
                        normText += L"raw:  " + url.raw + L"\r\n";
                        normText += L"          decoded: " + url.resolved + L"\r\n";
                    } else {
                        normText += url.resolved + L"\r\n";
                    }
                }
            }
            if (!hasUrl)
                normText += L"(없음)\r\n";

            // Shell Detection
            auto sh = ShellDetector::Detect(decoded);
            {
                normText += L"\r\n=== Shell ===\r\n";
                const wchar_t* name =
                    sh.shell == ShellDetector::SHELL_CMD        ? L"SHELL_CMD"        :
                    sh.shell == ShellDetector::SHELL_POWERSHELL ? L"SHELL_POWERSHELL" :
                                                                  L"SHELL_NONE";
                wchar_t buf[96];
                wsprintfW(buf, L"%s  (line %d, elevated=%s)\r\n",
                          name, (int)sh.lineIndex, sh.elevated ? L"true" : L"false");
                normText += buf;
            }

            // Parsed Commands - 셸 종류에 따라 라벨 변경
            const wchar_t* cmdLabel =
                sh.shell == ShellDetector::SHELL_CMD        ? L"CMD" :
                sh.shell == ShellDetector::SHELL_POWERSHELL ? L"PS"  :
                                                              L"CMD";  // SHELL_NONE 기본값

            normText += L"\r\n=== Parsed Commands ===\r\n";
            if (decoded.commands.empty()) {
                normText += L"(없음)\r\n";
            } else {
                for (auto& cmd : decoded.commands) {
                    wchar_t buf[32];
                    wsprintfW(buf, L"[line %d] ", (int)cmd.lineIndex);
                    normText += buf;
                    normText += cmdLabel;
                    normText += L": ";
                    normText += cmd.command;
                    if (!cmd.args.empty()) {
                        normText += L"  |  ARGS:";
                        for (auto& arg : cmd.args)
                            normText += L"  " + arg;
                    }
                    normText += L"\r\n";
                }
            }

            SetWindowTextW(hNorm, normText.c_str());
        }

        // --- Close button ---
        CreateWindowExW(0, L"BUTTON", L"Close",
            WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
            270, 440, 130, 35, hWnd, (HMENU)IDC_NORMDEBUG_CLOSE, hInst, nullptr);

        ShowWindow(hWnd, SW_SHOW);
        UpdateWindow(hWnd);
    }
}
