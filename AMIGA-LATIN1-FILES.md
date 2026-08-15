# Amiga Latin-1 files — ISO 8859-1 only (HARD RULE)

**All AI agents** (Cursor, Claude Code, Codex, etc.) must follow this when editing files in this repository.

**Where this rule is registered**

| Tool | Location |
|------|----------|
| Any agent | This file (`AMIGA-LATIN1-FILES.md`) — canonical source |
| Claude Code | `.claude/rules/amiga-latin1-files.md` (path-scoped; loads when touching covered files) |
| Cursor and other agents | `AGENTS.md` (repo root; Cursor reads this automatically) |
| Claude Code (summary) | `CLAUDE.md` |

These files run on or are displayed by real Amiga hardware, which does **not** support UTF-8.

## Covered files

**Bundle docs**

- `bundle/AmigaGPT/Install-AmigaGPT`
- `bundle/AmigaGPT/AmigaGPT.readme`
- `bundle/AmigaGPT/AmigaGPT.guide`

**C sources** — all `*.c` under `src/` (Amiga-facing strings, `$VER:` cookies, copyright symbols, and any future non-ASCII Latin-1 text)

**ARexx scripts** — all `*.rexx` under `bundle/` and `assets/` (ASCII-only today; must stay Latin-1-safe for future translated strings)

## Before and after every edit

Run this verification. **Do not commit** if any check fails:

```bash
python3 - <<'PY'
from pathlib import Path

FILES = [
    "bundle/AmigaGPT/Install-AmigaGPT",
    "bundle/AmigaGPT/AmigaGPT.readme",
    "bundle/AmigaGPT/AmigaGPT.guide",
] + sorted(Path("src").rglob("*.c")) + sorted(
    p for root in ("bundle", "assets") for p in Path(root).rglob("*.rexx")
)

def utf8_sequence_len(data: bytes, i: int) -> int:
    """Return 2–4 if data[i:] starts a valid UTF-8 multibyte sequence; else 0."""
    b = data[i]
    if b < 0x80:
        return 0
    if 0xC2 <= b <= 0xDF and i + 1 < len(data) and 0x80 <= data[i + 1] <= 0xBF:
        return 2
    if 0xE0 <= b <= 0xEF and i + 2 < len(data):
        if all(0x80 <= data[i + j] <= 0xBF for j in (1, 2)):
            return 3
    if 0xF0 <= b <= 0xF4 and i + 3 < len(data):
        if all(0x80 <= data[i + j] <= 0xBF for j in (1, 2, 3)):
            return 4
    return 0

for path in map(str, FILES):
    data = Path(path).read_bytes()
    if b"\xef\xbf\xbd" in data:
        raise SystemExit(f"{path}: UTF-8 replacement character U+FFFD (EF BF BD)")
    i = 0
    while i < len(data):
        n = utf8_sequence_len(data, i)
        if n > 1:
            raise SystemExit(f"{path}: UTF-8 multibyte sequence at offset {i}")
        i += 1
    print(f"OK: {path}")
PY
```

Also confirm charset: `file -I <path>` should report `iso-8859-1` or `us-ascii`.

## How to edit safely

- **Do not** use normal text patch tools (`StrReplace`, `Write`, etc.) on files that contain or will contain non-ASCII Latin-1 characters — they write UTF-8 and corrupt accented text.
- **Do** edit with `python3` using `read_bytes()` / `write_bytes()`, or restore bytes from a known-good git revision.
- Pure ASCII edits to `.rexx` / `.c` files are usually safe via normal tools, but **re-run verification** after any edit.
- If the user has the file open in an IDE, warn them to **reload or close the tab** after git operations so a stale buffer does not re-corrupt the file on save.

## Common corruption signs

- `ï¿½` in the file (UTF-8 bytes `EF BF BD`, replacement character)
- Curly quotes, em-dashes, or other UTF-8-only punctuation (`E2 80 xx` sequences)
- Accented letters stored as three-byte UTF-8 instead of one Latin-1 byte (e.g. `C3 B1` for ñ instead of `F1`)

## Fix

Replace UTF-8 sequences with the ISO 8859-1 single-byte equivalent, or plain ASCII. Re-run the verification script before committing.
