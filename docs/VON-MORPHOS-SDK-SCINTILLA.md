# Scintilla-Doku aus MorphOS-SDK (Workspace-Kopie)

**Stand:** 2026-05-20  
**Pfad:** `~/development/morphos/VonMorphosSDK/`

Vom Entwickler aus dem nativen SDK kopiert — **AmigaGuide**-Format, nicht HTML/PDF.

---

## Dateien im Ordner

| Datei / Ordner | Typ | Herkunft auf MorphOS |
|----------------|-----|----------------------|
| **`Scintilla.guide`** | AmigaGuide (`@database "Scintilla"`, `@Node`, `@EndNode`) | `Guide/Scintilla.guide` |
| **`Scintilla/`** | C-Header | `Guide/includes/Scintilla/` oder `gg/os-include/Scintilla/` |

Weitere Kopien (optional, siehe [MORPHOS-SDK-NATIV-UND-WSL.md](MORPHOS-SDK-NATIV-UND-WSL.md)): `Guide/includes/mui/Scintilla_mcc.h`, `Frameworks/.../MCCScintilla.h`.

---

## AmigaGuide lesen

| Umgebung | Vorgehen |
|----------|----------|
| **MorphOS** | AmigaGuide / MultiView / SDK-Hilfe — Hyperlinks `@{"…" Link "…"}` funktionieren |
| **WSL / Editor** | Plaintext; Knoten: `@Node Name "Titel"` … Inhalt … `@EndNode` |

**Schnellsuche im Editor:** `@Node MorphOS`, `SCI_SETTEXT`, `SCI_SETFONTQUALITY`, `SCI_SETLEXER`.

---

## Wichtige Knoten für AmigaGPT

| Knoten / Link-Ziel | Inhalt |
|--------------------|--------|
| **`--abouttheport--`** | MorphOS-Port (Nicholai Benalal, Scribble, Mitwirkende) |
| **`--introduction--`** | API-Konzept, `wParam`/`lParam`, MorphOS: `DoMethod(sciobj, m, wParam, lParam)` |
| **`MorphOS`** | `MUIC_Scintilla`, Notify (`SCIA_Notify`), SCIM_*-Methoden, Link TTEngine |
| **`SCI_SETTEXT`** | Text setzen: `SCI_SETTEXT(<unused>, const char *text)` |
| **`SCI_SETCODEPAGE`** | `SC_CP_UTF8` (65001) → Unicode-Modus, UTF-8-Dokument |
| **`SCI_SETFONTQUALITY`** | **0** = graphics.library (Default), **1** = **TTEngine** — „necessary for Unicode support“ |
| **`SCI_SETLEXER`** / **`SCI_SETLEXERLANGUAGE`** | Phase 11 Highlighting |

Auszug Knoten **MorphOS** (Zeilen ~8974–9017 in `Scintilla.guide`):

```c
Object *sciobj = MUI_NewObject(MUIC_Scintilla, TAG_DONE);
int caretpos = DoMethod(sciobj, SCI_GETCURRENTPOS, 0, 0);
```

Notify-Beispiel: `MUIM_Notify` auf `SCIA_Notify`, Handler liest `struct SCNotification *` aus `msg->scn`.

---

## TTEngine (Guide vs. AmigaGPT-Code)

Offizielle Doku (`SCI_SETFONTQUALITY`):

- Wert **1** schaltet **TTEngine** für Rendering ein (Unicode).
- `SCI_SETCODEPAGE` mit **`SC_CP_UTF8`** für UTF-8-Dokumente.

**AmigaGPT (`CodeBlocksScintilla.c` / `gui.c`):**

- `OpenLibrary("ttengine.library", …)` auf MorphOS
- `SCI_SETFONTQUALITY(1)` in `codeBlocksScintillaInitViewer` und `codeBlocksScintillaSetUtf8Text`
- `SCI_SETCODEPAGE` + `SC_CP_UTF8`
- `SCI_STYLESETFONT` / `SCI_STYLESETSIZE` auf `STYLE_DEFAULT` und Style `0` — Default **`DejaVu Sans Mono`**, 12 pt (`CODEBLOCKS_SCINTILLA_FONT_FACE` in `CodeBlocksScintilla.c`). Name **exakt** wie in Flow Studio (TTEngine an).

### TTEngine-Monospace (MorphOS / Flow Studio)

Nur **Monospace**-Familien für den Code-Viewer; Schreibweise aus Flow Studio übernehmen (oft mit „Mono“/„Sans Mono“ im Namen):

| Familie (typisch) | Hinweis |
|-------------------|--------|
| **DejaVu Sans Mono** | **Verbindlicher Default** — in Flow Studio mit UTF-Testdatei gegen Liberation/Noto/Roboto/Luxi u. a. geprüft; bei AmigaGPT beibehalten |
| **Bitstream Vera Sans Mono** | Alternative Monospace |
| **Liberation Mono** | „Liberation“ in der Liste |
| **Noto Sans Mono** / **Noto Mono** | Breites Unicode; für Emoji-Versuche zuerst testen (trotzdem oft □) |
| **Roboto Mono** | „Roboto“ + Mono |
| **Luxi Mono** | oft „Lux…“ in der Auswahl |
| Arimo | eher proportional — nur wenn explizit Mono-Variante |

**Nicht** die kurze MUI-Liste (Andale Mono, Arial, Times, Verdana) — das ist **graphics.library**, nicht TTEngine.

**Zwei Font-Welten:** MUI/graphics vs. TTEngine — siehe [UNICODE-MORPHOS-MUI.md](UNICODE-MORPHOS-MUI.md). Emoji: □ = fehlende Glyphe; `??` im Chat = noch NFloattext/codesets.

---

## Guide-API vs. `MUIP_Scintilla_Command` (AmigaGPT)

| Weg | Beschreibung | Status in AmigaGPT |
|-----|--------------|-------------------|
| **Guide / MorphOS-Knoten** | `DoMethod(sci, SCI_*, wParam, lParam)` — `SCI_*` als Method-ID | In Doku als Referenz; Code nutzt noch nicht |
| **Header `Scintilla_mcc.h`** | `struct MUIP_Scintilla_Command`, Slot **+2** | **`DoMethodA`** in `CodeBlocksScintilla.c` — auf MorphOS validiert |

Beide Varianten sind bei MCC-Ports üblich. Bei neuem Code oder Bugs zuerst den **Guide-Weg** testen; der struct-Weg bleibt dokumentiert in [MORPHOS-SDK-NATIV-UND-WSL.md](MORPHOS-SDK-NATIV-UND-WSL.md) §5.

---

## Phase 11 (Lexer) — Guide-Verweise

- `SCI_SETLEXER(int lexer)` — `SCLEX_*` aus `SciLexer.h` / `Scintilla.h`
- `SCI_SETLEXERLANGUAGE(<unused>, const char *name)` — z. B. `"python"`, `"c"` (case sensitive, siehe Knoten `SCI_SETLEXERLANGUAGE`)
- Nach Fence-Parser: Sprache aus `AICodeBlock.language` zu Lexer-Namen mappen

Header im Workspace: `VonMorphosSDK/Scintilla/SciLexer.h`.

---

## Siehe auch

- [MORPHOS-SDK-NATIV-UND-WSL.md](MORPHOS-SDK-NATIV-UND-WSL.md) — SDK nativ vs. WSL, `devfiles.txt`
- [SCINTILLA-ARCHITECTURE.md](SCINTILLA-ARCHITECTURE.md) — Phasen 6–13
