#include "compound_scorer.h"
#include <algorithm>
#include <iomanip>
#include <sstream>

// ──────────────────────────────────────────────────────────────────────
// CompoundScorer 구현
//
// 점수 정책 (점수체계_통합본.md):
//   final_score = w_TM × tm_score + w_URL × url_score
//                 + w_TS × typing_score + w_HK × hotkey_score
//   cap 10.0
//
// 가중치 — 임시값 (calibration 미정):
//   w_TM     = 1.0   ← ThreatMatcher (셸 명령 분석)
//   w_URL    = 0.0   ← UrlClassifier  미구현 (권장 시작값 0.2)
//   w_TS     = 0.0   ← TypingAnalyzer 미구현 (권장 시작값 0.3)
//   w_HK     = 0.5   ← HotkeyScorer (Win+R, Ctrl+Shift+Enter 등)
//                      → BadUSB 진입 시그널이라 보조 가중치로 합산.
//                        HotkeyScorer 자체가 cap 10 이라 0.5 가중치로 0~5 기여.
//   * 가중치 합 ≠ 1 일 때: 합산 후 cap 10.0
//
// 4단계 분류 임계값 — 임시값:
//   < 3.0   : SAFE
//   < 5.0   : NORMAL
//   < 7.5   : WARN
//   >= 7.5  : DANGER
// ──────────────────────────────────────────────────────────────────────

namespace CompoundScorer
{
    namespace {

        // ─── 가중치 (calibration 시 조정) ────────────────────────
        constexpr double W_TM     = 1.0;   // ThreatMatcher
        constexpr double W_URL    = 0.2;   // UrlClassifier
        constexpr double W_TYPING = 0.3;   // TypingAnalyzer
        constexpr double W_HOTKEY = 0.5;   // HotkeyScorer (보조)
        constexpr double SCORE_CAP = 10.0;

        // ─── 분류 임계값 ─────────────────────────────────────────
        constexpr double TH_SAFE   = 3.0;
        constexpr double TH_NORMAL = 5.0;
        constexpr double TH_WARN   = 7.5;

        Level ClassifyLevel(double final_score)
        {
            if (final_score <  TH_SAFE)   return SAFE;
            if (final_score <  TH_NORMAL) return NORMAL;
            if (final_score <  TH_WARN)   return WARN;
            return DANGER;
        }

    } // anonymous namespace


    // ─── Combine ────────────────────────────────────────────────────
    Verdict Combine(double tm_score, double url_score,
                     double typing_score, double hotkey_score)
    {
        Verdict v{};
        v.tm_score      = tm_score;
        v.url_score     = url_score;
        v.typing_score  = typing_score;
        v.hotkey_score  = hotkey_score;
        v.tm_weight     = W_TM;
        v.url_weight    = W_URL;
        v.typing_weight = W_TYPING;
        v.hotkey_weight = W_HOTKEY;

        double weighted = W_TM     * tm_score
                        + W_URL    * url_score
                        + W_TYPING * typing_score
                        + W_HOTKEY * hotkey_score;

        v.final_score  = (std::min)(SCORE_CAP, (std::max)(0.0, weighted));
        v.level        = ClassifyLevel(v.final_score);
        v.should_block = (v.level >= WARN);
        return v;
    }


    // ─── Level 라벨 ────────────────────────────────────────────────
    const wchar_t* LevelName(Level lv)
    {
        switch (lv) {
        case SAFE:   return L"안전";
        case NORMAL: return L"보통";
        case WARN:   return L"경고";
        case DANGER: return L"위험";
        default:     return L"?";
        }
    }


    // ─── FormatVerdict ─────────────────────────────────────────────
    std::wstring FormatVerdict(const Verdict& v)
    {
        std::wostringstream o;
        o << std::fixed << std::setprecision(3);
        const std::wstring sep(60, L'=');

        o << sep << L"\r\n"
          << L"CompoundScorer Verdict\n"
          << sep << L"\r\n"
          << L"  ThreatMatcher    : " << v.tm_score
          << L"  (weight " << v.tm_weight << L")\r\n"
          << L"  UrlClassifier    : " << v.url_score
          << L"  (weight " << v.url_weight << L")"
          << (v.url_weight == 0.0 ? L"  ← 미구현" : L"") << L"\r\n"
          << L"  TypingAnalyzer   : " << v.typing_score
          << L"  (weight " << v.typing_weight << L")"
          << (v.typing_weight == 0.0 ? L"  ← 미구현" : L"") << L"\r\n"
          << L"  HotkeyScorer     : " << v.hotkey_score
          << L"  (weight " << v.hotkey_weight << L")\r\n"
          << std::wstring(60, L'-') << L"\r\n"
          << L"  final_score      : " << v.final_score << L" / 10.0\r\n"
          << L"  level            : " << LevelName(v.level)
          << L" (" << static_cast<int>(v.level) << L")\r\n"
          << L"  should_block     : " << (v.should_block ? L"true" : L"false") << L"\r\n"
          << sep << L"\r\n";
        return o.str();
    }
}
