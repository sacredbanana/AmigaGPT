# Midi-Markdown & Chat-Export (Roadmap)

## Priorität

1. **Export Chat (raw UTF-8)** — **umgesetzt:** Ansicht → *Export chat (raw UTF-8)…* → `conversationNodeGetRaw()` pro Nachricht, inkl. ` ``` `; siehe `ChatExport.c`.
2. **Midi-Markdown (MorphOS):** `chatOutputScintillaBuildMidiMarkdownDisplay` — Marker werden **nicht** angezeigt (wie MUI), nur **Assistant**, `**` / `*` / `__`, `#`-Überschriften ohne `#`-Prefix; `[Codeblock …]` unverändert. **Emoji→Text** nur bei aktivem Menü *Markdown formatting* (Anzeige; `raw_utf8`/Export unverändert) — TTEngine/DejaVu ohne Farb-Emoji.
3. **Platzhalter `[Codeblock n]` → Code-Viewer (Klick):** Hotspot-Stil, `SCN_HOTSPOTCLICK` in [ChatOutputScintilla.c](../src/ChatOutputScintilla.c) → `codeBlocksViewerOpenAtIndex()`. Verworfene Ansätze: [PHASE-12-CHAT-SCINTILLA.md](PHASE-12-CHAT-SCINTILLA.md#3a--klick-auf-codeblock-n). Export/raw unverändert.
4. **Tabellen** (optional): eigener Block wie Code, **nach** Punkten 2–3 — nicht Spaltenausrichtung im Fließtext.

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
- Pipe-Tabellen mit gleichen Spaltenbreiten in Sans
- Vollständiges GFM
