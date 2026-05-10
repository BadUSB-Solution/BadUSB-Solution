#include "hotkey_scorer.h"
#include <algorithm>
#include <iomanip>
#include <sstream>
#include <unordered_map>
#include <cwctype>

// ──────────────────────────────────────────────────────────────────────
// HotkeyScorer 구현
//
// 점수 정책 (초기 calibration — 추후 운영 데이터로 조정):
//   각 핫키 조합에 고정 점수 부여 후 합산. Cap 10.0.
//
// 핫키 분류:
//   [Run]            Win+R                    3.0  ← BadUSB 거의 100% 첫 단계
//   [AdminElevation] Ctrl+Shift+Enter         4.0  ← Run 직후 관리자 권한 획득
//   [UAC-Confirm]    Alt+Y, Alt+예             2.0  ← UAC 자동 승인
//   [PowerUserMenu]  Win+X                    1.5  ← 관리자 메뉴 (디스크/터미널 등 접근)
//   [Search]         Win+S                    0.5  ← 검색 (저위험)
//   [Run-via-X]      Win+X 후 A (관리자), I    2.5  ← Win+X 서브메뉴 admin 진입
//   [LockHide]       Win+L                    1.5  ← 화면 잠금 (작업 은닉)
//   [Explorer]       Win+E                    1.0  ← 파일 탐색기 (파일 접근 패턴)
//   [Settings]       Win+I                    1.0  ← 설정 변경
//   [TaskMgr]        Ctrl+Shift+Esc           1.0  ← 작업관리자 (프로세스 조작)
//   [SecAttn]        Ctrl+Alt+Del             2.0  ← 보안 attention 시퀀스
//   [기타 Win+키]    Win+? (정의 안 된 것)     0.3  ← 약한 시그널만
//   [Ctrl+Shift+P 등] 그 외 일반              0    ← 정상 사용 가능, 점수 없음
//
// 추가 휴리스틱 (TODO — 추후 확장):
//   - 같은 핫키 반복 (예: Ctrl+S × 5) → 노이즈로 감점 또는 무시
//   - 짧은 시간 내 여러 핫키 시퀀스 → 가중치 ↑
//   - 정상 사용 패턴 (Ctrl+C, Ctrl+V 등) → 점수 0
// ──────────────────────────────────────────────────────────────────────

namespace HotkeyScorer
{
    namespace {

        constexpr double SCORE_CAP = 10.0;

        // 점수 + 분류 라벨
        struct Entry {
            double       score;
            std::wstring tag;
        };

        // 핫키 조합 → 점수 매핑.
        //   key 는 모두 lowercase 정규화 된 형태 ("win+r", "ctrl+shift+enter")
        //   대소문자 / +가 사이 공백 등 변형은 정규화 단계에서 처리.
        const std::unordered_map<std::wstring, Entry>& Table()
        {
            static const std::unordered_map<std::wstring, Entry> tbl = {
                // Run / 관리자 진입
                { L"win+r",              { 3.0, L"Run" } },
                { L"ctrl+shift+enter",   { 4.0, L"AdminElevation" } },
                // UAC 승인
                { L"alt+y",              { 2.0, L"UAC-Confirm" } },
                // Win+X PowerUserMenu (서브메뉴는 별도로 처리해도 되지만 1차로 base 만)
                { L"win+x",              { 1.5, L"PowerUserMenu" } },
                // 검색 / 잠금 / 탐색기 / 설정
                { L"win+s",              { 0.5, L"Search" } },
                { L"win+l",              { 1.5, L"LockHide" } },
                { L"win+e",              { 1.0, L"Explorer" } },
                { L"win+i",              { 1.0, L"Settings" } },
                // 작업관리자 / 보안 attention
                { L"ctrl+shift+esc",     { 1.0, L"TaskMgr" } },
                { L"ctrl+alt+delete",    { 2.0, L"SecAttn" } },
                { L"ctrl+alt+del",       { 2.0, L"SecAttn" } },
            };
            return tbl;
        }

        // 정규화: lowercase + 양쪽 공백 trim + "+" 주변 공백 제거
        //   "Win + R" → "win+r"
        //   "CTRL+SHIFT+Enter" → "ctrl+shift+enter"
        std::wstring Normalize(const std::wstring& s)
        {
            std::wstring out;
            out.reserve(s.size());
            for (wchar_t c : s) {
                if (c == L' ' || c == L'\t') continue;          // 공백 제거
                if (c >= L'A' && c <= L'Z') c = c - L'A' + L'a'; // lowercase
                out.push_back(c);
            }
            return out;
        }

        // Win+X 서브메뉴 키 매핑 (특히 위험한 항목만 점수 부여)
        //   A: Windows Terminal (Admin) 또는 Win11 의 Terminal (Admin) - 관리자 셸 진입
        //   I: PowerShell (구) - 관리자
        //   F: PowerShell - 일반
        //   K: PowerShell (Admin) (Win11)
        //   기타 (M, P, U 등): 일반 메뉴 — 점수 없음
        Entry WinXSubmenuScore(wchar_t k)
        {
            wchar_t lk = (k >= L'A' && k <= L'Z') ? k - L'A' + L'a' : k;
            switch (lk) {
            case L'a': return { 2.5, L"WinX-AdminTerminal" };
            case L'i': return { 2.5, L"WinX-AdminPS" };
            case L'k': return { 2.5, L"WinX-AdminPS" };
            case L'f': return { 1.0, L"WinX-PS" };
            default:   return { 0.0, L"" };
            }
        }

        // 미정의 Win+키 의 약한 점수 (Win+ 로 시작하지만 표에 없는 것들)
        //   예: Win+B, Win+D, Win+H, Win+N 등
        Entry GenericWinKeyScore(const std::wstring& norm)
        {
            // "win+x" 형태이고 x 가 한 글자
            if (norm.size() >= 5 && norm.substr(0, 4) == L"win+") {
                // 표에 있으면 위 Table() 에서 처리됨 → 여기 안 옴
                return { 0.3, L"Win-Generic" };
            }
            return { 0.0, L"" };
        }

    } // anon

    HotkeyResult Analyze(const std::vector<KeyDecoder::HotkeyEvent>& hotkeys)
    {
        HotkeyResult r{};
        const auto& tbl = Table();

        for (const auto& ev : hotkeys) {
            std::wstring key = Normalize(ev.combo);
            if (key.empty()) continue;

            HotkeyMatch m{};
            m.combo     = ev.combo;
            m.lineIndex = ev.lineIndex;

            // 1) 정의된 표 매칭
            auto it = tbl.find(key);
            if (it != tbl.end()) {
                m.score   = it->second.score;
                m.tag_key = it->second.tag;

                // Win+X 라면 서브메뉴 키 추가 점수
                if (key == L"win+x" && ev.submenuKey != 0) {
                    Entry sub = WinXSubmenuScore(ev.submenuKey);
                    if (sub.score > 0) {
                        m.score   += sub.score;
                        m.tag_key += L"+";
                        m.tag_key += sub.tag;
                    }
                }
            } else {
                // 2) Win+? 일반 (약한 시그널)
                Entry g = GenericWinKeyScore(key);
                if (g.score > 0) {
                    m.score   = g.score;
                    m.tag_key = g.tag;
                } else {
                    // 점수 없는 핫키 — 결과에 안 넣음 (노이즈 감소)
                    continue;
                }
            }

            r.raw_sum += m.score;
            r.matches.push_back(std::move(m));
        }

        r.final_score = (std::min)(SCORE_CAP, (std::max)(0.0, r.raw_sum));
        return r;
    }

    std::wstring FormatLog(const HotkeyResult& r)
    {
        std::wostringstream o;
        o << std::fixed << std::setprecision(2);
        const std::wstring sep(40, L'=');
        o << sep << L"\r\n"
          << L" HotkeyScorer 분석\r\n"
          << sep << L"\r\n";
        if (r.matches.empty()) {
            o << L"\r\n (감지된 위협 핫키 없음)\r\n";
        } else {
            size_t i = 0;
            for (const auto& m : r.matches) {
                ++i;
                o << L"\r\n[" << i << L"] line=" << m.lineIndex
                  << L"  " << m.combo
                  << L"  → " << m.tag_key
                  << L"  +" << m.score;
            }
            o << L"\r\n" << std::wstring(28, L'-') << L"\r\n"
              << L" raw 합     = " << r.raw_sum << L"\r\n"
              << L" final_score = " << r.final_score << L" / " << SCORE_CAP << L"\r\n";
        }
        o << sep << L"\r\n";
        return o.str();
    }
}
