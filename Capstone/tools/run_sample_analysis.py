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
OUT_PATH = ROOT / "검증결과_ThreatMatcher_CompoundScorer_샘플20.md"


spec = importlib.util.spec_from_file_location("prototype_threatmatcher", PROTO_PATH)
proto = importlib.util.module_from_spec(spec)
spec.loader.exec_module(proto)


SHELL_NONE = 0
SHELL_CMD = 1
SHELL_POWERSHELL = 2


@dataclass
class Decoded:
    lines: list[str]
    hotkeys: list[tuple[str, int]]
    commands: list[dict]
    shell_id: int


def extract_balanced_body(src: str, name: str) -> str:
    m = re.search(r"\bvoid\s+" + re.escape(name) + r"\s*\([^)]*\)\s*\{", src)
    if not m:
        return ""
    start = m.end()
    depth = 1
    i = start
    while i < len(src) and depth:
        if src[i] == "{":
            depth += 1
        elif src[i] == "}":
            depth -= 1
        i += 1
    return src[start:i - 1]


def split_statements(body: str) -> list[str]:
    out, cur, depth, quote, esc = [], [], 0, None, False
    for ch in body:
        cur.append(ch)
        if quote:
            if esc:
                esc = False
            elif ch == "\\":
                esc = True
            elif ch == quote:
                quote = None
            continue
        if ch in ("'", '"'):
            quote = ch
        elif ch in "({[":
            depth += 1
        elif ch in ")}]":
            depth = max(0, depth - 1)
        elif ch == ";" and depth == 0:
            out.append("".join(cur).strip())
            cur = []
    tail = "".join(cur).strip()
    if tail:
        out.append(tail)
    return out


def decode_c_string(s: str) -> str:
    return bytes(s, "utf-8").decode("unicode_escape")


def first_string_arg(stmt: str) -> str | None:
    m = re.search(r'(?:F\s*\(\s*)?"((?:\\.|[^"\\])*)"', stmt, re.S)
    return decode_c_string(m.group(1)) if m else None


def function_bodies(src: str) -> dict[str, str]:
    names = set(re.findall(r"\bvoid\s+([A-Za-z_]\w*)\s*\([^)]*\)\s*\{", src))
    return {name: extract_balanced_body(src, name) for name in names}


def process_body(body: str, funcs: dict[str, str], arg_map: dict[str, str] | None = None,
                 max_depth: int = 4) -> tuple[list[str], list[tuple[str, int]]]:
    arg_map = arg_map or {}
    lines = [""]
    hotkeys: list[tuple[str, int]] = []
    mods: set[str] = set()
    src = funcs.get("__src__", "")

    def write_text(text: str):
        lines[-1] += text

    def enter():
        lines.append("")

    def hotkey_for(key: str) -> str | None:
        key = key.strip()
        if key.startswith("'") and key.endswith("'"):
            key = key[1:-1]
        elif key.startswith("KEY_"):
            key = key[4:]
        key = key.upper()
        parts = []
        if "GUI" in mods:
            parts.append("Win")
        if "CTRL" in mods:
            parts.append("Ctrl")
        if "ALT" in mods:
            parts.append("Alt")
        if "SHIFT" in mods:
            parts.append("Shift")
        if not parts:
            return None
        parts.append(key[:1] if len(key) == 1 else key.title())
        return "+".join(parts)

    for stmt in split_statements(body):
        stmt = "\n".join(
            line for line in stmt.splitlines()
            if not line.lstrip().startswith("//")
        ).strip()
        if not stmt:
            continue

        call = re.match(r"([A-Za-z_]\w*)\s*\((.*)\)\s*;?$", stmt, re.S)
        if call and call.group(1) in funcs and max_depth > 0:
            name = call.group(1)
            params = re.search(r"\bvoid\s+" + re.escape(name) + r"\s*\(([^)]*)\)", src)
            local_map = {}
            arg = first_string_arg(stmt)
            if params and arg is not None:
                pm = re.search(r"([A-Za-z_]\w*)\s*(?:=.*)?$", params.group(1).strip())
                if pm:
                    local_map[pm.group(1)] = arg
            offset = len(lines) - 1
            sub_lines, sub_hotkeys = process_body(funcs[name], funcs, local_map, max_depth - 1)
            if sub_lines:
                lines[-1] += sub_lines[0]
                lines.extend(sub_lines[1:])
            hotkeys.extend((combo, offset + line_idx) for combo, line_idx in sub_hotkeys)
            continue

        m = re.search(r"(?:DigiKeyboard|Keyboard)\.println\s*\(", stmt)
        if m:
            text = first_string_arg(stmt)
            if text is not None:
                write_text(text)
            enter()
            continue

        if re.search(r"(?:DigiKeyboard|Keyboard)\.print\s*\(", stmt):
            text = first_string_arg(stmt)
            if text is None:
                var = re.search(r"\.print\s*\(\s*([A-Za-z_]\w*)\s*\)", stmt)
                text = arg_map.get(var.group(1), "") if var else ""
            write_text(text)
            continue

        if re.search(r"(?:DigiKeyboard|Keyboard)\.write\s*\(\s*KEY_RETURN\s*\)", stmt):
            enter()
            continue

        m = re.search(r"(?:DigiKeyboard|Keyboard)\.sendKeyStroke\s*\(([^)]*)\)", stmt)
        if m:
            args = [a.strip() for a in m.group(1).split(",")]
            key = args[0] if args else ""
            mod_arg = args[1] if len(args) > 1 else ""
            if "MOD_GUI" in mod_arg:
                hotkeys.append(("Win+" + key.replace("KEY_", "").title()[:1], len(lines) - 1))
            elif key in ("KEY_ENTER", "KEY_RETURN"):
                enter()
            continue

        m = re.search(r"(?:DigiKeyboard|Keyboard)\.press\s*\(([^)]*)\)", stmt)
        if m:
            key = m.group(1).strip()
            if "GUI" in key:
                mods.add("GUI")
            elif "ALT" in key:
                mods.add("ALT")
            elif "CTRL" in key or "CONTROL" in key:
                mods.add("CTRL")
            elif "SHIFT" in key:
                mods.add("SHIFT")
            elif key in ("KEY_RETURN", "KEY_ENTER") and not ({"GUI", "ALT"} & mods):
                enter()
            else:
                combo = hotkey_for(key)
                if combo:
                    hotkeys.append((combo, len(lines) - 1))
            continue

        if re.search(r"(?:DigiKeyboard|Keyboard)\.releaseAll\s*\(", stmt):
            mods.clear()

    return lines, hotkeys


def parse_source(path: Path) -> Decoded:
    src = path.read_text(encoding="utf-8", errors="ignore")
    funcs = function_bodies(src)
    funcs["__src__"] = src
    setup = funcs.get("setup", "")
    lines, hotkeys = process_body(setup, funcs)
    commands = []
    for i, line in enumerate(lines):
        for seg in proto._split_chain(line, SHELL_NONE):
            parsed = proto._segment_to_parsed(seg)
            if parsed and parsed.get("command"):
                parsed["line_index"] = i
                commands.append(parsed)

    shell_id = SHELL_NONE
    for combo, line_idx in hotkeys:
        if combo == "Win+R":
            for cmd in commands:
                if cmd["line_index"] == line_idx:
                    if cmd["command"] in ("cmd", "cmd.exe"):
                        shell_id = SHELL_CMD
                    elif cmd["command"] in ("powershell", "powershell.exe", "pwsh", "pwsh.exe"):
                        shell_id = SHELL_POWERSHELL
                    break
    if shell_id == SHELL_NONE:
        for cmd in commands:
            if cmd["command"] in ("cmd", "cmd.exe"):
                shell_id = SHELL_CMD
                break
            if cmd["command"] in ("powershell", "powershell.exe", "pwsh", "pwsh.exe"):
                shell_id = SHELL_POWERSHELL
                break
    return Decoded(lines, hotkeys, commands, shell_id)


def level(score: float) -> str:
    if score < 3.0:
        return "SAFE"
    if score < 5.0:
        return "NORMAL"
    if score < 7.5:
        return "WARN"
    return "DANGER"


def summarize_commands(result) -> str:
    items = []
    for cm in result.commands:
        if cm.cmd_raw > 0 or cm.in_db:
            items.append(f"{cm.command}({cm.cmd_raw:.3f})")
    return ", ".join(items[:8]) + (" ..." if len(items) > 8 else "")


def analyze_file(path: Path, label: str, matcher) -> dict:
    decoded = parse_source(path)
    result = proto.analyze(decoded.commands, matcher, shell_id=decoded.shell_id)
    final_score = min(10.0, max(0.0, result.session_score))
    return {
        "label": label,
        "name": path.parent.name,
        "path": path,
        "decoded": decoded,
        "result": result,
        "final_score": final_score,
        "level": level(final_score),
    }


def main():
    bad_files = sorted(BAD_ROOT.rglob("*.ino"))
    good_files = sorted(GOOD_ROOT.rglob("*.cpp"))
    bad_sel = random.Random(42).sample(bad_files, 10)
    good_sel = random.Random(42).sample(good_files, 10)

    matcher = proto.Matcher()
    rows = [analyze_file(p, "BadUSB", matcher) for p in bad_sel]
    rows += [analyze_file(p, "GoodUSB", matcher) for p in good_sel]

    md = []
    md.append("# ThreatMatcher + CompoundScorer 샘플 20개 검증 결과")
    md.append("")
    md.append("- 샘플 기준: `random.seed(42)`")
    md.append("- BadUSB: `BadUSB-Sample-main/**/*.ino` 중 10개")
    md.append("- GoodUSB: `GoodUSB-Sample-main/**/*.cpp` 중 10개")
    md.append("- 분석 경로: 샘플 입력 재구성 -> KeyDecoder 유사 command 추출 -> ThreatMatcher -> CompoundScorer")
    md.append("- CompoundScorer: 현재 C++ 구현 기준 `w_TM=1.0`, `w_URL=0.0`, `w_TYPING=0.0`이므로 final_score는 TM session_score와 동일")
    md.append("")
    md.append("## 요약")
    md.append("")
    md.append("| 구분 | # | 샘플 | TM session | Compound final | Level | Shell | tokens M/U | 핵심 매칭 |")
    md.append("|---|---:|---|---:|---:|---|---|---|---|")
    for idx, row in enumerate(rows, 1):
        r = row["result"]
        d = row["decoded"]
        shell = {0: "NONE", 1: "CMD", 2: "POWERSHELL"}[d.shell_id]
        rel = row["path"].relative_to(ROOT)
        md.append(
            f"| {row['label']} | {idx} | `{rel}` | {r.session_score:.3f} | "
            f"{row['final_score']:.3f} | {row['level']} | {shell} | "
            f"{r.matched_tokens}/{r.unmatched_tokens} | {summarize_commands(r)} |"
        )

    md.append("")
    md.append("## 상세")
    for idx, row in enumerate(rows, 1):
        r = row["result"]
        d = row["decoded"]
        rel = row["path"].relative_to(ROOT)
        md.append("")
        md.append(f"### {idx}. {row['label']} - {row['name']}")
        md.append("")
        md.append(f"- 파일: `{rel}`")
        md.append(f"- lines: {len([x for x in d.lines if x])}, hotkeys: {len(d.hotkeys)}, decoded commands: {len(d.commands)}")
        md.append(f"- shell: `{ {0: 'SHELL_NONE', 1: 'SHELL_CMD', 2: 'SHELL_POWERSHELL'}[d.shell_id] }`")
        md.append(f"- ThreatMatcher: raw={r.session_raw:.3f}, categories={len(r.activated_tags)}, session_score={r.session_score:.3f}")
        md.append(f"- CompoundScorer: final_score={row['final_score']:.3f}, level={row['level']}, should_block={str(row['level'] in ('WARN', 'DANGER')).lower()}")
        md.append(f"- SCIA axis counts: S={r.axis_counts['S']}, C={r.axis_counts['C']}, I={r.axis_counts['I']}, A={r.axis_counts['A']}")
        if r.activated_tags:
            md.append(f"- activated tags: {', '.join(sorted(r.activated_tags))}")
        md.append("")
        md.append("| command | in_db | phase | cmd_raw | token summary |")
        md.append("|---|---:|---:|---:|---|")
        for cm in r.commands:
            if not cm.in_db and cm.cmd_raw == 0:
                continue
            toks = []
            for tk in cm.tokens:
                tag = tk.tag_key or "unmatched"
                param = tk.param or "(self)"
                val = f"={tk.value}" if tk.value else ""
                toks.append(f"`{param}{val}` -> {tag}/{tk.axis}/{tk.token_score:.2f}")
            md.append(f"| `{cm.command}` | {str(cm.in_db).lower()} | {cm.phase} | {cm.cmd_raw:.3f} | {'; '.join(toks)} |")

    OUT_PATH.write_text("\n".join(md) + "\n", encoding="utf-8")
    print(OUT_PATH)


if __name__ == "__main__":
    main()
