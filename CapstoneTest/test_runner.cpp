#include "test_runner.h"
#include "keyboard_shortcuts_ref.h"
#include <sstream>
#include <iomanip>

namespace TestRunner
{
    PipelineResult Run(const std::vector<KeyboardHook::KeyRecord>& keys)
    {
        PipelineResult r{};
        r.decoded = KeyDecoder::Decode(keys);
        r.shell   = ShellDetector::Detect(r.decoded);
        r.threat  = ThreatMatcher::Analyze(r.decoded, r.shell.shell);
        r.url     = UrlClassifier::Classify(r.decoded);
        r.typing  = TypingAnalyzer::Analyze(keys);
        r.hotkey  = HotkeyScorer::Analyze(r.decoded.hotkeys);
        r.verdict = CompoundScorer::Combine(
            r.threat.session_score,
            r.url.final_score,
            r.typing.score,
            r.hotkey.final_score);
        return r;
    }

    static std::wstring FmtDouble(double v, int prec = 2)
    {
        std::wostringstream o;
        o << std::fixed << std::setprecision(prec) << v;
        return o.str();
    }

    std::wstring FormatResult(const std::vector<KeyboardHook::KeyRecord>& keys,
                               const PipelineResult& r)
    {
        std::wostringstream o;
        const std::wstring SEP(60, L'=');
        const std::wstring SUB(40, L'-');

        // ─── 키 입력 요약 ───
        o << SEP << L"\r\n"
          << L" Key Input\r\n"
          << SEP << L"\r\n"
          << L"  total events : " << keys.size() << L"\r\n";

        // ─── Lines ───
        o << L"\r\n" << SEP << L"\r\n"
          << L" Decoded Lines\r\n"
          << SEP << L"\r\n";
        for (size_t i = 0; i < r.decoded.lines.size(); ++i) {
            o << L"  [" << i << L"] " << r.decoded.lines[i].text << L"\r\n";
        }

        // ─── Hotkeys ───
        o << L"\r\n" << SEP << L"\r\n"
          << L" Hotkeys\r\n"
          << SEP << L"\r\n";
        if (r.decoded.hotkeys.empty()) {
            o << L"  (none)\r\n";
        } else {
            for (auto& h : r.decoded.hotkeys) {
                o << L"  " << h.combo;
                if (h.submenuKey != 0) {
                    auto entry = KeyShortcutRef::LookupWinXMenu(h.submenuKey);
                    if (entry) o << L" -> " << entry->action;
                }
                o << L"  @ line[" << h.lineIndex << L"]\r\n";
            }
        }

        // ─── URLs (raw 추출) ───
        o << L"\r\n" << SEP << L"\r\n"
          << L" URLs (extracted)\r\n"
          << SEP << L"\r\n";
        bool hasUrl = false;
        for (auto& line : r.decoded.lines) {
            for (auto& u : line.urls) {
                hasUrl = true;
                o << L"  [line " << u.lineIndex << L"] " << u.resolved;
                if (u.raw != u.resolved)
                    o << L"  (raw: " << u.raw << L")";
                o << L"\r\n";
            }
        }
        if (!hasUrl) o << L"  (none)\r\n";

        // ─── ShellDetector ───
        const wchar_t* shName =
            r.shell.shell == ShellDetector::SHELL_CMD        ? L"SHELL_CMD"        :
            r.shell.shell == ShellDetector::SHELL_POWERSHELL ? L"SHELL_POWERSHELL" :
                                                                L"SHELL_NONE";
        o << L"\r\n" << SEP << L"\r\n"
          << L" ShellDetector\r\n"
          << SEP << L"\r\n"
          << L"  shell    : " << shName << L"\r\n"
          << L"  line     : " << r.shell.lineIndex << L"\r\n"
          << L"  elevated : " << (r.shell.elevated ? L"true" : L"false") << L"\r\n";

        // ─── ParsedCommands ───
        o << L"\r\n" << SEP << L"\r\n"
          << L" Parsed Commands\r\n"
          << SEP << L"\r\n";
        if (r.decoded.commands.empty()) {
            o << L"  (none)\r\n";
        } else {
            const wchar_t* lbl =
                r.shell.shell == ShellDetector::SHELL_CMD        ? L"CMD" :
                r.shell.shell == ShellDetector::SHELL_POWERSHELL ? L"PS"  :
                                                                    L"CMD";
            for (const auto& c : r.decoded.commands) {
                o << L"  [line " << c.lineIndex << L"] " << lbl << L": " << c.command;
                if (!c.args.empty()) {
                    o << L"  |  ARGS:";
                    for (auto& a : c.args) o << L"  " << a;
                }
                o << L"\r\n";
            }
        }

        // ─── ThreatMatcher ───
        o << L"\r\n" << ThreatMatcher::FormatLog(r.threat);

        // ─── UrlClassifier ───
        o << L"\r\n" << SEP << L"\r\n"
          << L" UrlClassifier\r\n"
          << SEP << L"\r\n"
          << L"  final_score : " << FmtDouble(r.url.final_score) << L" / 10\r\n"
          << L"  URL count   : " << r.url.items.size() << L"\r\n";
        for (auto& it : r.url.items) {
            o << L"  [line " << it.lineIndex << L"] score="
              << FmtDouble(it.score) << L"  " << it.url << L"\r\n";
        }

        // ─── TypingAnalyzer ───
        const wchar_t* tcls =
            r.typing.classification == TypingAnalyzer::TYPING_AUTOMATED ? L"AUTOMATED" :
            r.typing.classification == TypingAnalyzer::TYPING_HUMAN     ? L"HUMAN"     :
                                                                          L"UNKNOWN";
        o << L"\r\n" << SEP << L"\r\n"
          << L" TypingAnalyzer\r\n"
          << SEP << L"\r\n"
          << L"  classification : " << tcls << L"\r\n"
          << L"  score          : " << FmtDouble(r.typing.score) << L" / 10\r\n"
          << L"  keys=" << r.typing.keyCount
          << L"  median=" << FmtDouble(r.typing.medianMs, 1) << L"ms"
          << L"  MAD="    << FmtDouble(r.typing.madMs, 1)    << L"ms"
          << L"  WPM="    << FmtDouble(r.typing.overallWpm, 0)
          << L"\r\n";

        // ─── HotkeyScorer ───
        o << L"\r\n" << HotkeyScorer::FormatLog(r.hotkey);

        // ─── CompoundScorer Verdict ───
        o << L"\r\n" << CompoundScorer::FormatVerdict(r.verdict);

        return o.str();
    }
}
