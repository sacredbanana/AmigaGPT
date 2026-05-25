# Midi-Markdown & Chat-Export (Roadmap)

## Priorität

1. **Export Chat (raw UTF-8)** — **umgesetzt:** Ansicht → *Export chat (raw UTF-8)…* → `conversationNodeGetRaw()` pro Nachricht, inkl. ` ``` `; siehe `ChatExport.c`.
2. Midi-Markdown: MD nur **außerhalb** Fences, nur **Assistant**, `**` / `*` / `__`, Überschriften `#`.
3. Platzhalter `[Codeblock n]` → Code-Viewer (Klick).
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
