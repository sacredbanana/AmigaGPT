# Phase 12 — Hauptfenster Chat-Ausgabe (Scintilla + TTEngine, MorphOS)

Ersetzt die große Chat-**Ausgabe** im Hauptfenster: NFloattext + `CodesetsUTF8ToStr` → read-only **Scintilla.mcc** mit **TTEngine** (`SC_CP_UTF8`). Motivation: [UNICODE-MORPHOS-MUI.md](UNICODE-MORPHOS-MUI.md) (Emoji → `??`).

## Entscheidungen (12.0)

| Thema | Umsetzung |
|-------|-----------|
| Layout | **Scrollgroup** + Scintilla (wie Code-Viewer); NListview-Wrapper und Phase-7c-Wheel entfallen auf MorphOS |
| Markdown | **Midi-Markdown** (Scintilla-Styles 2–8 auf Assistant); Menü `config.markdownFormatting` |
| User/Assistant | **Scintilla-Styles:** User = fett, dunkelblau, hellgrauer Hintergrund; Assistant = normal schwarz; `\n\n` zwischen Nodes |
| Font | **`config.fixedWidthFonts`**: an → DejaVu Sans Mono, aus → DejaVu Sans; Umschalten **ohne** Fenster-Neuaufbau (`applyFixedWidthFontsSetting`, nicht `RecreateMainWindow`) |

## Code

| Datei | Rolle |
|-------|--------|
| [ChatOutputScintilla.c](../src/ChatOutputScintilla.c) | Init, `SetUtf8Text`, Font; **3a offen:** Doppelklick → `codeBlocksViewerOpenAtIndex()` |
| [CodeBlocksViewer.c](../src/CodeBlocksViewer.c) | `codeBlocksViewerOpenAtIndex(ULONG)` — API da, noch **nicht** vom Chat aus aufgerufen |
| [MainWindow.c](../src/MainWindow.c) | Layout, `displayConversation`, Stream, `sendChatMessage`, Clear |
| [CodeBlocksScintilla.c](../src/CodeBlocksScintilla.c) | `codeBlocksScintillaCommand` (SCI_*) |

Hilfsfunktionen (nur MorphOS): `chatOutputUpdateFromBuffer()`, `clearChatOutputDisplay()`.

## Menüs auf MorphOS (12.0)

- **Markdown formatting** — **Midi-Markdown** ([MIDI-MARKDOWN-ROADMAP.md](MIDI-MARKDOWN-ROADMAP.md)); kein `SCLEX_MARKDOWN` im Chat.
- **Export chat (raw UTF-8)…** — siehe `ChatExport.c`, [MIDI-MARKDOWN-ROADMAP.md](MIDI-MARKDOWN-ROADMAP.md).
- **User / Assistant text alignment** — No-op (Hervorhebung über Role-Styles)

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
8. **Export chat (raw)** → Datei mit `raw_utf8`, Fences sichtbar
9. **Markdown formatting** an: `**fett**`, `*kursiv*`, `__unterstrichen__`, `# Überschrift` nur bei Assistant; Emoji Assistant z. B. 🌍→`[Welt]`, 👍→`(+1)` (nur Anzeige)
10. **Markdown formatting** aus: Emoji unverändert (oft □)
11. Alignment umschalten → kein Absturz (No-op)

## Phase 12.1 / Midi-Markdown

Siehe [MIDI-MARKDOWN-ROADMAP.md](MIDI-MARKDOWN-ROADMAP.md) — kein `SCLEX_MARKDOWN` im Chat.
- Scintilla-Styling für Alignment
- Dediziertes Chat-Font-Menü
- Stream nur Append-Delta (Vorbereitung Phase 13)

## 3a — Doppelklick auf `[Codeblock n]` (offen)

**Ziel:** Doppelklick auf die Platzhalter-Zeile im Chat → Code-Blocks-Viewer öffnen, Block `n` aktiv (`codeBlocksViewerOpenAtIndex`). **Export/raw unverändert.**

**Stand:** `codeBlocksViewerOpenAtIndex()` in [CodeBlocksViewer.c](../src/CodeBlocksViewer.c) implementiert; Anbindung aus [ChatOutputScintilla.c](../src/ChatOutputScintilla.c) fehlt noch.

### Constraint (nicht wieder verletzen)

- Chat-Scintilla bleibt **`ScintillaObject` im `WindowObject`-Macro** in [MainWindow.c](../src/MainWindow.c) — gleiche Einrückung wie Commit `0cda730` (`MUIA_Scrollgroup_Contents` → `chatOutputTextEditor = ScintillaObject, …`).
- **Kein** vorgefertigtes Objekt, **keine** Variable/Klasse im Macro, **kein** nachträgliches `Child` mit Pointer-Tag.
- Doppelklick nur über **Notify / SCI-Handler / Fenster-EventHandler** am bestehenden Objekt — nach `createMainWindow()` bzw. in `chatOutputScintillaInitViewer()` oder Hook-Registrierung.

### Verworfene Ansätze (MorphOS, 2026-05)

| Ansatz | Ergebnis |
|--------|----------|
| Scintilla vorab mit `NewObject`, dann `Child, chatOutputTextEditor` im `WindowObject` | MUI: „Ungültiger Zahlenwert“, danach „Hauptfenster konnte nicht erstellt werden“ (Pointer als Tag) |
| `MUI_NewObject((STRPTR)variableClass, …)` im `WindowObject`-Macro | Macro bricht (`End` / Compile-Fehler) |
| Eigene MUI-Klasse `MUIC_AmigaGPTChatOutputScintilla` im gleichen Macro-Kontext | Gleiche Fehlerklasse wie oben |
| `set(MUIA_Scrollgroup_Contents, prebuiltObject)` nach Fensterbau | Instabiler Start (Fenster/Startup-Probleme) |
| Subclass + `MUIM_HandleEvent` am Chat-Scintilla (eigene Klasse) | Zurückgerollt mit Chat-Output-Restore |
| `chatOutputScintillaInitViewer()` **nach** `MUIA_Window_Open` / `OM_ADDMEMBER` | Boot-Log lief durch (`boot …` bis `NewInput loop start`), **Hauptfenster unsichtbar**; Fix: Init wieder **vor** `OM_ADDMEMBER` wie `0cda730` |

### Startup (nicht 3a, aber gleiche Session)

- **`streamLogBootPhase`:** voller Boot-Pfad im Stream-Log hilft, „Log ja, Fenster nein“ von echtem Init-Fehler zu trennen.
- **Icon-Start (`cli == NULL`):** `GetMsg` + frühes `ReplyMsg` in [main.c](../src/main.c) — nur für diesen Startweg relevant; bei Shell-Start prüfen, ob überhaupt `WBStartup` anliegt (nicht von AmigaOS-3.1-„Workbench“-Docs auf MorphOS übernehmen).

### Noch zu prüfen (wenn 3a umgesetzt wird)

- Scintilla.mcc: `MUIA_Scintilla_Notify` / `SCN_*` / `MM_SciHandler` (MorphOS-SDK / Scintilla.mcc-Doku)
- EventHandler am Hauptfenster, falls Notify nicht reicht
- Nach Lösung: diesen Abschnitt um **„Gewählte Lösung“** + Testpunkt im Testplan ergänzen
