# Phase 12 — Hauptfenster Chat-Ausgabe (Scintilla + TTEngine, MorphOS)

**Status (2026-05):** Phase **12** und **12.1 (Midi-Markdown)** auf MorphOS **abgeschlossen** (Hardware-Abnahme). Phase **13** (Worker/UI-Batching) zurückgestellt — siehe [STREAM-RECOVERY.md](STREAM-RECOVERY.md) R3.

Ersetzt die große Chat-**Ausgabe** im Hauptfenster: NFloattext + `CodesetsUTF8ToStr` → read-only **Scintilla.mcc** mit **TTEngine** (`SC_CP_UTF8`). Motivation: [UNICODE-MORPHOS-MUI.md](UNICODE-MORPHOS-MUI.md) (Emoji → `??`).

## Entscheidungen (12.0)

| Thema | Umsetzung |
|-------|-----------|
| Layout | **Scrollgroup** + Scintilla (wie Code-Viewer); NListview-Wrapper und Phase-7c-Wheel entfallen auf MorphOS |
| Markdown | **Midi-Markdown** (Scintilla-Styles 2–8, 11 auf Assistant); Menü `config.markdownFormatting` |
| User/Assistant | **Scintilla Role-Styles** (kein Ausrichtungs-Menü): User = fett, dunkelblau, hellgrauer Hintergrund; Assistant = schwarz; `\n\n` zwischen Nodes. OS3/OS4: weiter NFloattext + Menü *User/Assistant text alignment* |
| Font | **`config.fixedWidthFonts`** (Ansicht: *Feste Schriftbreite*) + **`config.chatFontSize`** (*Chat-Schrift vergrößern/verkleinern*) |

## Code

| Datei | Rolle |
|-------|--------|
| [ChatOutputScintilla.c](../src/ChatOutputScintilla.c) | Init, `SetUtf8Text`, Font; **3a:** Codeblock-Hotspot → deferred `codeBlocksViewerOpenAtIndexWithToken()`; **3b:** URL-Hotspot → `URL_OpenA`; gemeinsam: `SCIA_Notify`, `SCN_HOTSPOTRELEASECLICK`, `SCI_GETSTYLEAT`, `SCN_HOTSPOTCLICK` + `SCI_CANCEL` |
| [CodeBlocksViewer.c](../src/CodeBlocksViewer.c) | `codeBlocksViewerOpenAtIndex` / `OpenAtIndexWithToken`, `Dismiss`, Menü `ScheduleOpenWindow` |
| [MainWindow.c](../src/MainWindow.c) | Layout, `displayConversation`, Stream, `sendChatMessage`, Clear |
| [CodeBlocksScintilla.c](../src/CodeBlocksScintilla.c) | `codeBlocksScintillaCommand` (SCI_*) |

Hilfsfunktionen (nur MorphOS): `chatOutputUpdateFromBuffer()`, `clearChatOutputDisplay()`.

## Menüs auf MorphOS (12.0)

Nur unter `#ifdef __MORPHOS__` in `menu.c` — OS3/OS4/AROS haben weiter NFloattext-Chat ohne diese Einträge.

- **Markdown formatting** — **Midi-Markdown** ([MIDI-MARKDOWN-ROADMAP.md](MIDI-MARKDOWN-ROADMAP.md)); kein `SCLEX_MARKDOWN` im Chat.
- **Export chat (raw UTF-8)…** — siehe `ChatExport.c`, [MIDI-MARKDOWN-ROADMAP.md](MIDI-MARKDOWN-ROADMAP.md).
- **Wrap long lines (chat)** — `SCI_SETWRAPMODE` (`SC_WRAP_WORD` / `SC_WRAP_NONE`); `config.chatLineWrap` in `config.json`; `chatOutputScintillaApplyLineWrap()` auch nach Font-Wechsel
- **Feste Schriftbreite** — `config.fixedWidthFonts` → Chat-Scintilla Mono vs. Sans
- **Chat-Schrift vergrößern/verkleinern** — `config.chatFontSize`
- **Increase / Decrease chat font size** — `config.chatFontSize` (8–24 pt)

## Nicht in Phase 12

- Chat-**Eingabe** (`TextEditor`)
- NList-Titel (`name_list_display`)
- Code-Viewer-Fenster
- OS3/OS4 Chat-Ausgabe (NFloattext unverändert); dort Menü *User/Assistant text alignment* — auf MorphOS **entfallen** (Role-Styles reichen)
- Phase **13** (Worker / UI-Batching)

## MorphOS-Testplan

**Abnahme:** MorphOS 2026-05 — Punkte 1–14 durchgetestet (Hotspots, Markdown-Toggle inkl. langer Chats, Stream, Restart). Bei größeren Änderungen Testplan erneut ab Punkt 1.

1. Build + `package-morphos-cross.sh` → Z:
2. Emoji Assistant: Markdown aus, „Antworte nur mit: 😀“ → kein `??`
3. Emoji/Umlaut User: Eingabe senden → sofort korrekt in der Ausgabe
4. **New Chat** / **Delete Chat**: leere Ausgabe, kein Crash
5. Langer Stream: live + vollständig nach Ende (R1/R2)
6. Code-Fence-Platzhalter im Chat; Code-Viewer unverändert
7. **Feste Schriftbreite** an/aus → Chat Mono vs. Sans
8. **Export chat (raw)** → Datei mit `raw_utf8`, Fences sichtbar
9. **Markdown formatting** an: `**fett**`, `*kursiv*`, `__unterstrichen__`, `` `inline code` `` (Mono, grauer Hintergrund), `#`–`######` Überschriften (H1 größer … H6 leicht größer, nur Assistant); Emoji Assistant z. B. 🌍→`[Welt]`, 👍→`(+1)` (nur Anzeige); `x**2` bleibt literal (kein Fett); `Wenn`s` / `Wenn's` mit Grave-Accent **kein** grauer Rest der Zeile; `` `Januar` `` weiterhin Inline-Code; innerhalb `` `...` `` keine `*`/`**`/Links/Emoji
10. **Markdown formatting** aus: Emoji unverändert (oft □)
11. User vs. Assistant visuell unterscheidbar (Role-Styles: User fett/blau/grau, Assistant schwarz)
12. **[Codeblock n]** (3a): blauer Link → Code-Viewer, Block `n` aktiv
13. **Bare URL + Markdown-Link** (3b): `https://…` klickbar; `[Siehe hier](https://…)` zeigt nur Label, Klick öffnet URL; `([Label](https://…))` mit äußeren Klammern sichtbar
14. **Pipe-Tabelle** (3c): Assistant-Tabelle nach Antwort-Ende ausgerichtet; Mono besser als Sans; Export/raw enthält ungepaddete Markdown-Tabelle

## Phase 12.1 / Midi-Markdown — abgeschlossen

Siehe [MIDI-MARKDOWN-ROADMAP.md](MIDI-MARKDOWN-ROADMAP.md) — kein `SCLEX_MARKDOWN` im Chat.

- ~~User/Assistant text alignment (Scintilla)~~ **bewusst entfallen** — Role-Styles statt NFloattext-Ausrichtung; Menü nur OS3/OS4 (`#ifndef __MORPHOS__` in `menu.c`)
- ~~Chat-Font-Menü~~ **erledigt** — *Feste Schriftbreite* + Schriftgröße +/− (nur Chat-Scintilla)
- ~~Stream nur Append-Delta~~ **erledigt** — live Stream: `SCI_APPENDTEXT` + Styling nur für Delta; nach Ende weiterhin volles `displayConversation` (Markdown/Hotspots)
- ~~Inline code `` `...` ``~~ **erledigt** — Style 11 (Mono + grauer Hintergrund); nur bei *Markdown formatting*
- ~~Bare `http(s)://`-Links (**3b**)~~ **erledigt** — Hotspot + `openurl.library`; Markdown `[Label](url)`; Abschnitt [3b](#3b-url-hotspots-openurl) unten
- ~~Pipe-Tabellen (**3c**)~~ **erledigt** — `chatOutputScintillaFormatPipeTables()`; Abschnitt [3c](#3c-pipe-tables) unten

## 3a — Klick auf `[Codeblock n]`

**Ziel:** Ein Klick auf die Platzhalter-Zeile im Chat (Hotspot-Link) → Code-Blocks-Viewer öffnen, Block `n` aktiv (`codeBlocksViewerOpenAtIndex`). **Export/raw unverändert.**

**Gewählte Lösung (MorphOS):** `[Codeblock n]`-Zeilen mit Scintilla-**Hotspot**-Stil (Link-Optik). Viewer erst bei **`SCN_HOTSPOTRELEASECLICK`** (Maus-Up), nicht bei `SCN_HOTSPOTCLICK` — sonst bleibt das Hauptfenster/Scrollgroup auf Mouse-Down hängen (Zieh-Scroll). `PushMethod` + Fenster-EH auf Button-Up: `SCI_CANCEL` am Chat-Scintilla. `MM_SciHandler` = `MUIA_Scintilla_dummy + 9`.

### Constraint (nicht wieder verletzen)

- Chat-Scintilla bleibt **`ScintillaObject` im `WindowObject`-Macro** in [MainWindow.c](../src/MainWindow.c) — gleiche Einrückung wie Commit `0cda730` (`MUIA_Scrollgroup_Contents` → `chatOutputTextEditor = ScintillaObject, …`).
- **Kein** vorgefertigtes Objekt, **keine** Variable/Klasse im Macro, **kein** nachträgliches `Child` mit Pointer-Tag.
- Klick nur über **Hotspot + Notify / SCI-Handler** am bestehenden Objekt — in `chatOutputScintillaInitViewer()` / `AttachNotify`.

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

### MorphOS-Test (3a)

12. Ein Klick auf `[Codeblock n]` (blauer Link) in der Chat-Ausgabe → Code-Blocks-Fenster, Block `n` aktiv (Menü „Codeblöcke“ weiterhin ok).

<a id="3b-url-hotspots-openurl"></a>

## 3b — URL-Hotspots (OpenURL)

**Ziel:** **Bare** `http://` / `https://` sowie Markdown **`[Beschriftung](http…)`** und optional umschlossen **`([Beschriftung](http…))`** in der Chat-Ausgabe (User und Assistant): sichtbarer Text als Link-Hotspot, **Maus-Up** → `openurl.library`. **Export/raw unverändert** (`raw_utf8` in den Nodes unverändert).

**Umsetzung (MorphOS):** In `chatOutputScintillaBuildMidiMarkdownDisplay()` werden `http`/`https`-Links in Klammer-Syntax erkannt; in der Anzeige erscheint nur die **Beschriftung** (bei leerem `[]` die URL selbst). Die Ziel-URL liegt in einem kleinen **Span-Cache** (`chatOutputMdLinkSpans`), damit `chatOutputScintillaOpenUrlAtSciPos()` auch ohne sichtbare URL im Text öffnen kann. Zusätzlich markiert `chatOutputScintillaApplyUrlHotspotStyles()` weiterhin **bare** URLs im fertigen UTF-8. Scintilla-Stil **`CHAT_OUTPUT_STYLE_URL_HOTSPOT`**, gleicher Notify-Pfad wie **3a** (`SCN_HOTSPOT*` + `SCI_CANCEL`).

**Menü *Markdown formatting*:** Aus = keine `**`/`#`/Emoji-Ersetzung, aber **Link-Syntax** und bare URLs werden weiter aufgelöst (`config.markdownFormatting` steuert nur Midi-Markdown-Stripping).

### Abgrenzung

- Nur Ziele mit Schema **`http://`** oder **`https://`** (kein `mailto:`, keine relativen Pfade).
- URL in Klammern endet am **ersten** `)` (keine `)` in der URL ohne Encoding — typische GPT-Links ok).
- Ohne funktionierendes **OpenURL** / Browser passiert beim Klick nichts (kein Fehlerdialog in AmigaGPT vorgesehen).

### MorphOS-Test (3b)

Siehe Testplan **Punkt 13** oben. Zusätzlich: Zeile nur `[Codeblock n]` darf nach wie vor **3a** sein (URL-Scanner überschreibt keine Codeblock-Zeilen).

<a id="3c-pipe-tables"></a>

## 3c — Pipe-Tabellen (Anzeige)

**Ziel:** Einfache GFM-Pipe-Tabellen in Assistant-Antworten lesbar ausrichten (Spalten per UTF-8-Zeichenanzahl mit Leerzeichen auffüllen). **Export/raw unverändert.**

**Pipeline:** `chatOutputScintillaBuildMidiMarkdownDisplay()` (Links, optional Midi-Markdown) → **`chatOutputScintillaFormatPipeTables()`** → Scintilla. Tabellen **nicht** während Live-Stream (`morphosChatStreamRawScintillaRefresh`). Link-Span-Positionen werden nach Padding angepasst.

**Performance:** Zeilenanfang und Heading-`contentStart` werden pro physikalischer Zeile nur einmal berechnet (nicht pro Byte), damit sehr lange Zeilen beim Chat-Wechsel nicht O(n²) blockieren.

**Regeln:** Header + Trennzeile `|---|` + Datenzeilen; gleiche Spaltenzahl; keine `\|` in Zellen. **GFM mit führendem/abschließendem `|`** (leere Rand-Spalte) wird erkannt — Trennzeile darf dabei leere Zellen enthalten, solange mindestens ein Segment `---` enthält. In der **Anzeige** wird die Trennzeile **neu erzeugt**: pro Spalte genau so viele `-` wie die breiteste Zelle in dieser Spalte (UTF‑8-Zeichenanzahl), damit die Linie optisch zu den Daten passt. Durchgängig leere **trailing** Spalten (typisch nur vom abschließenden `|` in **jeder** Zeile inkl. Trennzeile) werden nicht mit ausgegeben — nicht anhand von `colWidths` gekürzt (sonst gingen Spalten verloren, die nur in der Trennzeile `---` haben). Trennzeile: führende/abschließende `:` der GFM-Ausrichtung bleiben erhalten, Innenraum mit `-` auf die Zielbreite aufgefüllt. User-Text und `[Codeblock n]`-Zeilen unverändert. *Fixed width fonts* aus = Sans (Näherung); an = Mono (empfohlen).

### MorphOS-Test (3c)

14. Assistant-Antwort mit Pipe-Tabelle nach Stream-Ende: Spalten visuell ausgerichtet (Mono); Link in Tabellenzelle `[Label](https://…)` klickbar nach Padding.

### Constraint (wie 3a)

- Kein zweites Scintilla außerhalb des `WindowObject`-Macros; nur **Hotspot + bestehendes** `SCIA_Notify`-Sink-Objekt.
