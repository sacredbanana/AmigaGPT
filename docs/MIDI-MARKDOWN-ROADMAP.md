# /home/weimer/development/morphos/AmigaGPT & Chat-Export (Roadmap)

## Priorität

1. **Export Chat (raw UTF-8)** — **umgesetzt:** Ansicht → *Export chat (raw UTF-8)…* → `conversationNodeGetRaw()` pro Nachricht, inkl. ` ``` `; siehe `ChatExport.c`.
2. **Midi-Markdown (MorphOS)** — **umgesetzt:** `chatOutputScintillaBuildMidiMarkdownDisplay` — Marker werden **nicht** angezeigt (wie MUI), nur **Assistant**, `` `inline` ``, `**` / `*` / `__`, ATX-Überschriften `#`–`######` ohne `#`-Prefix mit **größerer Schrift** (H1–H6, `SCI_STYLESETSIZE`); `[Codeblock …]` unverändert. **Emoji→Text** nur bei aktivem Menü *Markdown formatting* (Anzeige; `raw_utf8`/Export unverändert) — TTEngine/DejaVu ohne Farb-Emoji.
3. **Platzhalter `[Codeblock n]` → Code-Viewer** — **umgesetzt:** Hotspot-Stil; Öffnen bei **`SCN_HOTSPOTRELEASECLICK`** (Maus-Up), `SCN_HOTSPOTCLICK` nur für Auswahl abbrechen; `codeBlocksViewerOpenAtIndexWithToken()` + Epoch beim Chat-Wechsel. Details: [PHASE-12-CHAT-SCINTILLA.md](PHASE-12-CHAT-SCINTILLA.md#3a--klick-auf-codeblock-n). Export/raw unverändert.
4. **Bare `http://` / `https://` + Markdown `[label](http…)` / `([label](http…))`** — **umgesetzt:** **3b** in [PHASE-12-CHAT-SCINTILLA.md](PHASE-12-CHAT-SCINTILLA.md#3b-url-hotspots-openurl): Hotspot-Stil `CHAT_OUTPUT_STYLE_URL_HOTSPOT`; Klick wie **3a** (Release) → **`openurl.library`** (`URL_OpenA`). Anzeige nur **Label** (oder URL bei leerem Label); Ziel-URL in Span-Cache. **Nur** `http`/`https`-Ziele; Markdown `[text](url)` bleibt in `raw_utf8`/Export. Ohne OpenURL/Browser passiert nichts. Link-Auflösung **immer** aktiv (auch wenn *Markdown formatting* aus ist); `**`/Emoji-Stripping nur bei *Markdown formatting* an.
5. **Tabellen** — **umgesetzt (Anzeige):** einfache GFM-Pipe-Tabellen (Header + `|---|`-Zeile + Datenzeilen, keine `\|` in Zellen). Spalten per **Zeichenanzahl** mit Leerzeichen aufgefüllt — **immer** in der Chat-Ansicht (nicht nur bei *Fixed width fonts*); Mono = saubere Ausrichtung, Sans = Näherung. **Nur Assistant**, nicht in `[Codeblock …]`-Zeilen; **nicht** während Stream (R3). Läuft **nach** Link-Auflösung in `chatOutputScintillaFormatPipeTables()`; `raw_utf8`/Export unverändert.
6. **Zeilenumbruch (Chat)** — **umgesetzt (nur MorphOS):** Ansicht → *Wrap long lines (chat)*; `config.chatLineWrap` → `chatOutputScintillaApplyLineWrap()` (`SC_WRAP_WORD` / `SC_WRAP_NONE`). OS3/OS4/AROS: Chat weiter NFloattext — **kein** Menüpunkt, kein Port geplant (nur MorphOS setzt Chat-Scintilla ein). Details: [PHASE-12-CHAT-SCINTILLA.md](PHASE-12-CHAT-SCINTILLA.md) (Menüs 12.0).

**Performance (Chat-Wechsel):** Heading-Erkennung in `chatOutputScintillaBuildMidiMarkdownDisplay()` pro physikalischer Zeile, nicht pro Byte (vermeidet O(n²) auf langen Zeilen bei aktivem *Markdown formatting*).

### Delimiter-Policy (keine Markdown-Bibliothek)

Parser-Reihenfolge in `chatOutputScintilla.c` / `MainWindow.c` (`parseMarker` / `chatMdParseMarker`): `` ` `` → `__` → `**` → einzelnes `*`. Style-Stack für Ein/Aus; Escape `\`, `\*`, `\_`. Innerhalb `` `...` `` keine weiteren Marker, Links oder Emoji-Ersetzung.

| Marker | Verhalten | Begründung |
|--------|-----------|------------|
| **Kursiv** `*` | CommonMark-ähnlich: `chatmd_markers.c` — *left/right-flanking*, Wort-intern (`Spieler*innen`) aus, Heuristik Multiplikation (`8 *9=444`), kein Marker bei `\*` / `'*'`, Fußnote nur exakt `(*)`. Open/Close über Stack (`chatMdItalicStarCanOpen` / `CanClose`). Escape im Chat: `\` + nächstes Zeichen literal (`ChatOutputScintilla.c` / `MainWindow.c`). | Ein `*` kommt oft „falsch“ vor (Mathe, Wörter, Doku mit Anführungs-Stern). |
| **Fett** `**` | `chatMdBoldDoubleStarCanOpen` / `CanClose`: Open mit left-flanking (nicht `**.`, nicht `x**2`/`a**10`); Close wenn Bold im Stack (auch nach `.` wie `satzende.**`). `chatMdPopBoldRun` beendet verschachtelte Reste. | Schließen nach Satzzeichen; kein Fett bei Potenz-Notation. |
| **Unterstreichen** `__` | **Naiv:** jedes `__` = Toggle (wie Fett). | Gleiche pragmatische Linie wie Fett. |
| **Inline-Code** `` ` `` | Einzelnes Backtick öffnet/schließt nur mit **schließendem** `` ` `` in derselben Zeile; `Wenn`s` (Apostroph-Heuristik) und verwaiste `` ` `` bleiben literal. Mono + grauer Hintergrund. Nur bei *Markdown formatting* an; ` ``` ` (Fence) bleibt literal. | Verhindert „alles grau“ nach `Wenn`s`; echte `` `Januar` `` ok. |

Host-Tests (WSL, ohne MorphOS): Kursiv/Fett-`*` inkl. `x**2` — `tools/test-chatmd-markers.sh`; Code-Fences (3+ Backticks) — `tools/test-codefence.sh`.

Bekannte Einschränkung (MorphOS/Scintilla.mcc): Beim Wechsel zwischen Chat-Scintilla und Code-Scintilla kann Rest-Scroll/Wheel-Zustand übernommen werden ("Scroll-Vererbung"). Gleiches Verhalten ist auch in anderen Scintilla-Apps auf MorphOS reproduzierbar; aggressive App-Workarounds wurden zurückgebaut.

## Export-Format (Diagnose)

```text
# AmigaGPT chat export (raw UTF-8)
# conversation: <Titel>

--- user ---
<raw_utf8>

--- assistant ---
<raw_utf8>
```

`system`-Rollen werden übersprungen (wie `chat-history.json`). Nicht exportiert: `display_text` mit Platzhaltern.

## Nicht geplant (v0.1)

- `SCLEX_MARKDOWN` im Chat (MorphOS-Lexer fehlt)
- **User/Assistant text alignment** im Chat-Scintilla (MorphOS) — Role-Styles statt NFloattext-Ausrichtung; Menü nur OS3/OS4
- **Zeilenumbruch / Midi-Markdown / Hotspots** auf OS3/OS4 im Chat (dort kein Scintilla in der Ausgabe)
- Pipe-Tabellen mit gleichen Spaltenbreiten in Sans (nur Näherung; Mono empfohlen)
- Vollständiges GFM
