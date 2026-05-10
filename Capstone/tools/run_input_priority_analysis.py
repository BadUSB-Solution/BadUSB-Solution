import importlib.util
import math
import random
import re
from dataclasses import dataclass
from pathlib import Path


ROOT = Path(r"C:\Users\hanwoo\Desktop\mapping")
BAD_ROOT = ROOT / "BadUSB-Sample-main"
GOOD_ROOT = ROOT / "GoodUSB-Sample-main"
PROTO_PATH = ROOT / "prototype_threatmatcher.py"
OUT_PATH = ROOT / "검증결과_현재버전_input우선_샘플20.md"
SEED = 20260510


spec = importlib.util.spec_from_file_location("prototype_threatmatcher", PROTO_PATH)
proto = importlib.util.module_from_spec(spec)
spec.loader.exec_module(proto)


SHELL_NONE = 0
SHELL_CMD = 1
SHELL_POWERSHELL = 2

HOTKEY_WEIGHTS = {
    "win+r": (3.0, "Run"),
    "ctrl+shift+enter": (4.0, "AdminElevation"),
    "alt+y": (2.0, "UAC-Confirm"),
    "win+x": (1.5, "PowerUserMenu"),
    "win+s": (0.5, "Search"),
    "win+l": (1.5, "LockHide"),
    "win+e": (1.0, "Explorer"),
    "win+i": (1.0, "Settings"),
    "ctrl+shift+esc": (1.0, "TaskMgr"),
    "ctrl+alt+delete": (2.0, "SecAttn"),
    "ctrl+alt+del": (2.0, "SecAttn"),
}

SHIFT_CHARS = {
    "0": ")", "1": "!", "2": "@", "3": "#", "4": "$",
    "5": "%", "6": "^", "7": "&", "8": "*", "9": "(",
    "-": "_", "=": "+", ";": ":", "'": '"', ",": "<",
    ".": ">", "/": "?", "[": "{", "]": "}", "\\": "|",
    "`": "~",
}


@dataclass
class DecodedInput:
    lines: list[str]
    hotkeys: list[dict]
    commands: list[dict]
    shell_id: int


def rel(path: Path) -> str:
    return str(path.relative_to(ROOT))


def read_text(path: Path) -> str:
    for enc in ("utf-8-sig", "utf-8", "cp949"):
        try:
            return path.read_text(encoding=enc)
        except UnicodeDecodeError:
            pass
    return path.read_text(encoding="utf-8", errors="ignore")


def tokenize_input(text: str) -> list[str]:
    return re.findall(r"\S+", text)


def key_label(tok: str) -> str:
    t = tok.strip()
    aliases = {
        "Enter": "Enter",
        "Return": "Enter",
        "Esc": "Esc",
        "Escape": "Esc",
        "Delete": "Delete",
        "Del": "Del",
        "Backspace": "Backspace",
        "Tab": "Tab",
        "F12": "F12",
    }
    if t in aliases:
        return aliases[t]
    if len(t) == 1:
        return t.upper()
    return t


def shifted(tok: str) -> str:
    if len(tok) == 1 and tok.isalpha():
        return tok.upper()
    return SHIFT_CHARS.get(tok, tok)


def parse_input_file(path: Path) -> DecodedInput:
    tokens = tokenize_input(read_text(path))
    lines = [""]
    hotkeys = []
    mods = []

    def consume_combo(key: str):
        nonlocal mods
        combo = "+".join([*mods, key_label(key)])
        hotkeys.append({"combo": combo, "line_index": len(lines) - 1, "submenu": ""})
        if key_label(key) == "Enter" and "Win" not in mods and "Alt" not in mods:
            lines.append("")
        mods = []

    for tok in tokens:
        low = tok.lower()
        if low in ("win", "gui"):
            mods.append("Win")
            continue
        if low in ("ctrl", "control"):
            mods.append("Ctrl")
            continue
        if low == "alt":
            mods.append("Alt")
            continue
        if low == "shift":
            mods.append("Shift")
            continue
        if low == "space":
            if any(m in mods for m in ("Win", "Ctrl", "Alt")):
                consume_combo("Space")
            else:
                lines[-1] += " "
                mods = [m for m in mods if m not in ("Shift",)]
            continue
        if low in ("enter", "return"):
            if any(m in mods for m in ("Win", "Ctrl", "Alt")):
                consume_combo("Enter")
            else:
                lines.append("")
                mods = []
            continue
        if low == "tab":
            if any(m in mods for m in ("Win", "Ctrl", "Alt")):
                consume_combo("Tab")
            else:
                lines[-1] += "\t"
                mods = []
            continue
        if low in ("backspace", "bksp"):
            if lines[-1]:
                lines[-1] = lines[-1][:-1]
            mods = []
            continue

        if any(m in mods for m in ("Win", "Ctrl", "Alt")):
            consume_combo(tok)
            continue

        if "Shift" in mods:
            lines[-1] += shifted(tok)
        else:
            lines[-1] += tok.lower() if len(tok) == 1 and tok.isalpha() else tok
        mods = []

    commands = []
    for i, line in enumerate(lines):
        for seg in proto._split_chain(line, SHELL_NONE):
            parsed = proto._segment_to_parsed(seg)
            if parsed and parsed.get("command"):
                parsed["line_index"] = i
                commands.append(parsed)

    shell_id = detect_shell(commands, hotkeys)
    return DecodedInput(lines=lines, hotkeys=hotkeys, commands=commands, shell_id=shell_id)


def detect_shell(commands: list[dict], hotkeys: list[dict]) -> int:
    run_lines = {h["line_index"] for h in hotkeys if h["combo"].lower().replace(" ", "") == "win+r"}
    for cmd in commands:
        if cmd["line_index"] in run_lines:
            c = cmd["command"].lower()
            if c in ("cmd", "cmd.exe"):
                return SHELL_CMD
            if c in ("powershell", "powershell.exe", "pwsh", "pwsh.exe"):
                return SHELL_POWERSHELL
    for cmd in commands:
        c = cmd["command"].lower()
        if c in ("cmd", "cmd.exe"):
            return SHELL_CMD
        if c in ("powershell", "powershell.exe", "pwsh", "pwsh.exe"):
            return SHELL_POWERSHELL
    return SHELL_NONE


def hotkey_score(hotkeys: list[dict]) -> tuple[float, list[dict]]:
    raw = 0.0
    matches = []
    for h in hotkeys:
        norm = h["combo"].lower().replace(" ", "")
        score, tag = HOTKEY_WEIGHTS.get(norm, (0.0, ""))
        if score == 0.0 and norm.startswith("win+"):
            score, tag = 0.3, "Win-Generic"
        if score > 0.0:
            raw += score
            matches.append({"combo": h["combo"], "score": score, "tag": tag, "line_index": h["line_index"]})
    return min(10.0, max(0.0, raw)), matches


def final_score(tm_score: float, hk_score: float) -> float:
    return min(10.0, max(0.0, tm_score + 0.5 * hk_score))


def level(score: float) -> str:
    if score < 3.0:
        return "SAFE"
    if score < 5.0:
        return "NORMAL"
    if score < 7.5:
        return "WARN"
    return "DANGER"


def source_file_for(input_path: Path) -> Path | None:
    candidates = sorted(input_path.parent.glob("*.ino")) + sorted(input_path.parent.glob("*.cpp"))
    return candidates[0] if candidates else None


def summarize_source(input_text: str, source_text: str, decoded: DecodedInput) -> str:
    words = []
    lower = input_text.lower() + "\n" + source_text.lower()
    if "powershell" in lower or "pwsh" in lower:
        words.append("PowerShell 실행")
    if re.search(r"\bcmd\b|cmd\.exe", lower):
        words.append("CMD 실행")
    if any(x in lower for x in ("curl", "iwr", "invoke-webrequest", "wget")):
        words.append("외부 다운로드/웹 요청")
    if any(x in lower for x in ("reg add", "reg delete", "set-itemproperty")):
        words.append("레지스트리 변경")
    if any(x in lower for x in ("sc.exe", "set-service", "stop-service", "start-service")):
        words.append("서비스 제어")
    if any(x in lower for x in ("send-mailmessage", "smtp", "mail")):
        words.append("메일 전송")
    if any(x in lower for x in ("remove-item", "del ", "delete")):
        words.append("파일/흔적 삭제")
    if any(x in lower for x in ("start-process", "wscript", "mshta", "schtasks")):
        words.append("프로세스/스크립트 실행")
    if not words:
        if decoded.hotkeys and not any(line.strip() for line in decoded.lines):
            words.append("단축키 기반 정상 동작")
        elif decoded.commands:
            words.append("일반 명령/앱 실행")
        else:
            words.append("키 입력 또는 LED/보조 동작 위주")
    return ", ".join(dict.fromkeys(words))


def command_summary(result) -> str:
    items = []
    for cm in result.commands:
        if cm.in_db or cm.cmd_raw > 0:
            items.append(f"{cm.command}({cm.cmd_raw:.3f})")
    return ", ".join(items[:8]) + (" ..." if len(items) > 8 else "")


def token_summary(cm) -> str:
    parts = []
    for tk in cm.tokens[:8]:
        param = tk.param or "(self)"
        val = f"={tk.value}" if tk.value else ""
        tag = tk.tag_key or "unmatched"
        parts.append(f"`{param}{val}` -> {tag}/{tk.axis}/{tk.token_score:.2f}")
    if len(cm.tokens) > 8:
        parts.append("...")
    return "; ".join(parts)


def collect_inputs(root: Path, label: str) -> list[Path]:
    if label == "BadUSB":
        return sorted(root.rglob("*_input.txt"))
    return sorted(root.rglob("input.txt"))


def analyze_one(input_path: Path, label: str, matcher) -> dict:
    decoded = parse_input_file(input_path)
    tm = proto.analyze(decoded.commands, matcher, shell_id=decoded.shell_id)
    hk, hk_matches = hotkey_score(decoded.hotkeys)
    final = final_score(tm.session_score, hk)
    src_path = source_file_for(input_path)
    input_text = read_text(input_path)
    source_text = read_text(src_path) if src_path else ""
    return {
        "label": label,
        "input": input_path,
        "source": src_path,
        "decoded": decoded,
        "tm": tm,
        "hk": hk,
        "hk_matches": hk_matches,
        "final": final,
        "level": level(final),
        "summary": summarize_source(input_text, source_text, decoded),
    }


def main():
    bad_inputs = collect_inputs(BAD_ROOT, "BadUSB")
    good_inputs = collect_inputs(GOOD_ROOT, "GoodUSB")
    rng = random.Random(SEED)
    selected = [(p, "BadUSB") for p in rng.sample(bad_inputs, 10)]
    selected += [(p, "GoodUSB") for p in rng.sample(good_inputs, 10)]

    matcher = proto.Matcher()
    rows = [analyze_one(path, label, matcher) for path, label in selected]

    md = []
    md.append("# 현재 버전 ThreatMatcher + CompoundScorer input 우선 샘플 20개 검증")
    md.append("")
    md.append(f"- 샘플링: `random.seed({SEED})`")
    md.append("- BadUSB: `BadUSB-Sample-main/**/*_input.txt` 중 10개")
    md.append("- GoodUSB: `GoodUSB-Sample-main/**/input.txt` 중 10개")
    md.append("- 분석 우선순위: input 파일을 실제 키 입력으로 디코딩하고, 같은 폴더의 `.ino/.cpp`는 코드 내용 요약 보조로 사용")
    md.append("- 현재 CompoundScorer 계산식: `final_score = min(10, TM + 0.5 * Hotkey)` (`URL=0`, `Typing=0`)")
    md.append("")
    md.append("## 전체 요약")
    md.append("")
    md.append("| 구분 | # | input 파일 | 코드 요약 | TM | Hotkey | 최종 | Level | Shell | 핵심 매칭 |")
    md.append("|---|---:|---|---|---:|---:|---:|---|---|---|")
    for i, row in enumerate(rows, 1):
        shell = {0: "NONE", 1: "CMD", 2: "POWERSHELL"}[row["decoded"].shell_id]
        md.append(
            f"| {row['label']} | {i} | `{rel(row['input'])}` | {row['summary']} | "
            f"{row['tm'].session_score:.3f} | {row['hk']:.3f} | {row['final']:.3f} | "
            f"{row['level']} | {shell} | {command_summary(row['tm'])} |"
        )

    md.append("")
    md.append("## 상세 결과")
    for i, row in enumerate(rows, 1):
        decoded = row["decoded"]
        tm = row["tm"]
        src = f"`{rel(row['source'])}`" if row["source"] else "없음"
        shell = {0: "SHELL_NONE", 1: "SHELL_CMD", 2: "SHELL_POWERSHELL"}[decoded.shell_id]
        md.append("")
        md.append(f"### {i}. {row['label']} - {row['input'].parent.name}")
        md.append("")
        md.append(f"- input 파일: `{rel(row['input'])}`")
        md.append(f"- 참조 코드: {src}")
        md.append(f"- 코드 내용 요약: {row['summary']}")
        md.append(f"- 디코딩: lines={len([x for x in decoded.lines if x.strip()])}, hotkeys={len(decoded.hotkeys)}, commands={len(decoded.commands)}, shell=`{shell}`")
        md.append(f"- ThreatMatcher 계산: raw={tm.session_raw:.3f}, categories={len(tm.activated_tags)}, session_score={tm.session_score:.3f}")
        if len(tm.activated_tags) > 0:
            md.append(f"  - 공식: `min(10, {tm.session_raw:.3f} / sqrt({len(tm.activated_tags)})) = {tm.session_score:.3f}`")
        else:
            md.append("  - 공식: 활성 tag 없음 -> `0.000`")
        md.append(f"- HotkeyScorer 계산: hotkey_score={row['hk']:.3f}")
        if row["hk_matches"]:
            hk_bits = ", ".join(f"{m['combo']}({m['tag']} +{m['score']:.1f})" for m in row["hk_matches"])
            md.append(f"  - 매칭: {hk_bits}")
        else:
            md.append("  - 매칭: 없음")
        md.append(f"- CompoundScorer 최종: `min(10, {tm.session_score:.3f} + 0.5 * {row['hk']:.3f}) = {row['final']:.3f}` -> **{row['level']}**")
        md.append(f"- SCIA axis counts: S={tm.axis_counts['S']}, C={tm.axis_counts['C']}, I={tm.axis_counts['I']}, A={tm.axis_counts['A']}")
        if tm.activated_tags:
            md.append(f"- activated tags: {', '.join(sorted(tm.activated_tags))}")
        md.append("")
        md.append("| command | in_db | phase | cmd_raw | token summary |")
        md.append("|---|---:|---:|---:|---|")
        wrote = False
        for cm in tm.commands:
            if not cm.in_db and cm.cmd_raw == 0:
                continue
            wrote = True
            md.append(f"| `{cm.command}` | {str(cm.in_db).lower()} | {cm.phase} | {cm.cmd_raw:.3f} | {token_summary(cm)} |")
        if not wrote:
            md.append("| - | - | - | 0.000 | 매칭된 명령 없음 |")

    OUT_PATH.write_text("\n".join(md) + "\n", encoding="utf-8")
    print(str(OUT_PATH))


if __name__ == "__main__":
    main()
