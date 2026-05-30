# Phase 11 — Syntax-Highlighting (Code-Viewer)

Branch **scintilla**, MorphOS. Ergänzt [SCINTILLA-ARCHITECTURE.md](SCINTILLA-ARCHITECTURE.md) Phase 11.

## Umsetzung

| API | Verwendung |
|-----|------------|
| `SCI_SETLEXER` / `SCI_SETLEXERLANGUAGE` | Builtin-Lexer, wenn `SCI_GETLEXER` den erwarteten Wert liefert |
| `SCI_COLOURISE` | Nach `SCI_SETTEXT` (nur bei aktivem Builtin-Lexer) |
| `SCI_SETSTYLINGEX` | **Fallback**, wenn kein Builtin-Lexer (typisch auf MorphOS) |

**Code:** `src/CodeBlocksScintilla.c`

MorphOS **Scintilla.mcc** ist oft **ohne** eingebaute SciLexer-Module gebaut (Guide: nur mit `SCI_LEXER` beim Build). Dann: einfacher **Fallback-Highlighter** (Kommentare grün, Strings rot, Zahlen teal) per `SCI_SETSTYLINGEX` — C-ähnlich (`//`, `/* */`, `"`) oder Python (`#`, `'`, `"`).

**Auslöser:** `codeBlocksViewerShowActiveBlock()` → `AICodeBlock.language`.

## Unterstützte Fence-Tags (Auswahl)

| Tag | Lexer |
|-----|--------|
| `python`, `py` | Python |
| `c`, `cpp`, `java`, `js`, … | C/C++-Lexer (`SCLEX_CPP`) |
| `json` | JSON |
| `shell`, `bash`, `sh` | Bash |
| `lua` | Lua |
| `html`, `xml`, `css` | Web |
| `sql`, `ruby`, `rust`, `php`, `perl`, `r`, `yaml`, `markdown`, `rexx` | jeweils passendes `SCLEX_*` |

Vollständige Liste: Tabelle `codeBlocksLexerMap[]` in `CodeBlocksScintilla.c`.

## MorphOS-Test

1. LHA deployen, App neu starten.
2. Chat mit Antwort, die **mehrere** Fences enthält, z. B. ` ```python ` und ` ```c `.
3. **View code blocks…** — NList zeigt `index:language`.
4. Block wechseln: Scintilla zeigt **farbige** Syntax (nicht nur Plain-UTF-8).
5. Copy/Save UTF-8 unverändert (Rohbytes aus `raw_code`, kein Highlighting in der Datei).

**Gegenprobe:** Fence ohne Sprache (` ``` `) → Plain-Text (kein Lexer).

## Nicht in Phase 11

- Tabs für mehrere Blöcke (optional später)
- ~~Inline-Code im Chat~~ — **Phase 12.1** (`` `...` `` in Chat-Scintilla)
- ~~Chat-Hauptfenster-Scintilla (**Phase 12**)~~ — **erledigt** — [PHASE-12-CHAT-SCINTILLA.md](PHASE-12-CHAT-SCINTILLA.md)

---

*Stand: 2026-05-28 — Phase 11 minimal (Lexer im Code-Viewer); Chat-Features siehe Phase 12.*
