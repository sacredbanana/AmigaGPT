# Amiga Latin-1 files — ISO 8859-1 only (HARD RULE)

**All AI agents** (Cursor, Claude Code, Codex, etc.) must follow this when editing files in this repository.

**Where this rule is registered**

| Tool | Location |
|------|----------|
| Any agent | This file (`AMIGA-LATIN1-FILES.md`) — canonical source |
| Claude Code | `.claude/rules/amiga-latin1-files.md` (path-scoped; loads when touching covered files) |
| Cursor and other agents | `AGENTS.md` (repo root; Cursor reads this automatically) |
| Claude Code (summary) | `CLAUDE.md` |
| Cursor / VS Code editor | `.vscode/settings.json` (per-language `files.encoding`) |

These files run on or are displayed by real Amiga hardware, which does **not** support UTF-8.

## Covered files

**Bundle docs**

- `bundle/AmigaGPT/Install-AmigaGPT`
- `bundle/AmigaGPT/AmigaGPT.readme`
- `bundle/AmigaGPT/AmigaGPT.guide`

**C sources** — all `*.c` under `src/` (Amiga-facing strings, `$VER:` cookies, copyright symbols, and any future non-ASCII Latin-1 text)

**ARexx scripts** — all `*.rexx` under `bundle/` and `assets/` (ASCII-only today; must stay Latin-1-safe for future translated strings)

## Editor setup (Cursor / VS Code)

`.vscode/settings.json` pins the encoding so these files open and save as ISO 8859-1 without
"Reopen with Encoding":

- `"[c]"` — covers every `*.c` under `src/`, plus the headers that `files.associations` maps to
  `c` (VS Code treats an unlisted `.h` as `cpp`).
- `"[lisp]"` + `files.associations` mapping `Install-AmigaGPT` to `lisp` — the installer is
  Lisp-shaped, so this gives it real highlighting as well as the encoding. It needs the
  `slbtty.lisp-syntax` extension (recommended in `.vscode/extensions.json`); without it the
  language id does not exist and the file falls back to plain text and UTF-8.
- `"[ini]"` + `files.associations` mapping `*.guide` and `*.readme` to `ini` — those are prose
  with no language of their own, and `files.encoding` can only be overridden per language, so
  they borrow a built-in language id that nothing else in this repo uses.
- `"[rexx]"` — applies only while a Rexx language extension is installed; nothing provides a
  `rexx` language out of the box, so by default `*.rexx` opens as plain text in UTF-8. Those
  files are ASCII-only today, and the verification script below is what actually guards them.
- `"files.autoGuessEncoding": false` — the encoding is decided by the rules above, not guessed.

The `charset = latin1` entries in `.editorconfig` are kept for other editors and tools, but VS
Code and Cursor do not read `.editorconfig` themselves — that needs the EditorConfig extension,
so those entries alone do not set the encoding.

To confirm it is working, open one of the files and check the status bar reads **ISO 8859-1**.

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
