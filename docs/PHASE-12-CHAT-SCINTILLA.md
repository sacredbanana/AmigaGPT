# Phase 12 — Hauptfenster Chat-Ausgabe (Scintilla + TTEngine, MorphOS)

Ersetzt die große Chat-**Ausgabe** im Hauptfenster: NFloattext + `CodesetsUTF8ToStr` → read-only **Scintilla.mcc** mit **TTEngine** (`SC_CP_UTF8`). Motivation: [UNICODE-MORPHOS-MUI.md](UNICODE-MORPHOS-MUI.md) (Emoji → `??`).

## Entscheidungen (12.0)

| Thema | Umsetzung |
|-------|-----------|
| Layout | **Scrollgroup** + Scintilla (wie Code-Viewer); NListview-Wrapper und Phase-7c-Wheel entfallen auf MorphOS |
| Markdown | **Plain UTF-8**; Menü „Markdown formatting“ bleibt, wirkt auf MorphOS in 12.0 nicht |
| User/Assistant | **Fließtext**, `\n\n` zwischen Nodes; keine `\033`-Ausrichtung/Fettdruck |
| Font | **`config.fixedWidthFonts`**: an → DejaVu Sans Mono, aus → DejaVu Sans; Umschalten **ohne** Fenster-Neuaufbau (`applyFixedWidthFontsSetting`, nicht `RecreateMainWindow`) |

## Code

| Datei | Rolle |
|-------|--------|
| [ChatOutputScintilla.c](../src/ChatOutputScintilla.c) | Init, `SetUtf8Text`, Font |
| [MainWindow.c](../src/MainWindow.c) | Layout, `displayConversation`, Stream, `sendChatMessage`, Clear |
| [CodeBlocksScintilla.c](../src/CodeBlocksScintilla.c) | `codeBlocksScintillaCommand` (SCI_*) |

Hilfsfunktionen (nur MorphOS): `chatOutputUpdateFromBuffer()`, `clearChatOutputDisplay()`.

## Menüs auf MorphOS (12.0) — bewusst No-op

Einträge und `config.json` bleiben (OS3/OS4 + spätere 12.1). Umschalten ruft weiter `displayConversation(NULL)` auf; **sichtbar** ändert sich auf MorphOS in 12.0 nicht:

- **Markdown formatting** — kein Scintilla-Markdown-Lexer in 12.0
- **User / Assistant text alignment** — kein Scintilla-Styling in 12.0

## Nicht in Phase 12

- Chat-**Eingabe** (`TextEditor`)
- NList-Titel (`name_list_display`)
- Code-Viewer-Fenster
- OS3/OS4 Chat-Ausgabe (NFloattext unverändert)
- Phase **13** (Worker / UI-Batching)

## MorphOS-Testplan

1. Build + `package-morphos-cross.sh` → Z:
2. Emoji Assistant: Markdown aus, „Antworte nur mit: 😀“ → kein `??`
3. Emoji/Umlaut User: Eingabe senden → sofort korrekt in der Ausgabe
4. **New Chat** / **Delete Chat**: leere Ausgabe, kein Crash
5. Langer Stream: live + vollständig nach Ende (R1/R2)
6. Code-Fence-Platzhalter im Chat; Code-Viewer unverändert
7. **Fixed width fonts** an/aus → Mono vs. Sans
8. Alignment/Markdown umschalten → Anzeige unverändert, kein Absturz

## Phase 12.1 (optional)

- `SCLEX_MARKDOWN` wenn `markdownFormatting` an
- Scintilla-Styling für Alignment
- Dediziertes Chat-Font-Menü
- Stream nur Append-Delta (Vorbereitung Phase 13)
