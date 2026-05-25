# Architektur- und Implementierungsplan: AmigaGPT + Scintilla.mcc (MorphOS)

Dieses Dokument beschreibt die Zielarchitektur für den Branch **scintilla** und darüber hinaus. Es ergänzt die Git-Hinweise in [GIT-FORK-WORKFLOW.md](GIT-FORK-WORKFLOW.md).

Zeichensatz, NFloattext, MorphOS-Prüfung (`??` bei Emoji) und Cairo-Abwägung: [UNICODE-MORPHOS-MUI.md](UNICODE-MORPHOS-MUI.md).

SDK nativ (MorphOS) vs. WSL, `MUIM_Scintilla_Command`, Inventar `devfiles.txt`: [MORPHOS-SDK-NATIV-UND-WSL.md](MORPHOS-SDK-NATIV-UND-WSL.md). AmigaGuide-Kopie: [VON-MORPHOS-SDK-SCINTILLA.md](VON-MORPHOS-SDK-SCINTILLA.md) (`morphos/VonMorphosSDK/Scintilla.guide`).

---

## Roadmap (Übersicht)

| Phase | Status / Inhalt |
|-------|-----------------|
| **1–5** | Ziel, Datenmodell, Stream, Fences, GUI-Grundlagen |
| **6–10** | Scintilla **Code-Viewer** v0.1 — **6, 7a, 7b, 7c erledigt** (MorphOS, 2026-05); **8–10** offen (Safety/Logs/DoD) |
| **R1–R4** | **Stream- & Chat-Recovery** — [STREAM-RECOVERY.md](STREAM-RECOVERY.md) (parallel zu 7c/8–10, vorrangig R1+R2) |
| **11** | Komfort am Code-Viewer (Lexer, Highlighting, Tabs, Export) |
| **12** | **Hauptfenster Chat-Ausgabe** → Scintilla.mcc + TTEngine (UTF-8, ohne NFloattext/codesets für diese Fläche) |
| **13** | MorphOS Worker / UI-Batching für Stream (bewusst **nach** Phase 12) |

**Bereits umgesetzt, unverändert in Phase 12:** NList-Titel über `name_list_display` + codesets (Phase 5.1). **Nicht** in 6–12: Chat-**Eingabe** (`TextEditor`), Cairo.

---

## Phase 1 — Zieldefinition und Minimal-Architektur

**Ziel zuerst:**

- stabile AI-Chat-Ausgabe
- verlustfreie Code-Darstellung
- robustes UTF-8
- korrektes Copy/Paste
- möglichst wenig Magie

**NICHT zuerst:**

- Syntax Highlighting
- komplexes Markdown
- HTML
- Richtext
- Themes
- fancy UI

Das spart Zeit und verhindert frühe Architekturfehler.

---

## Phase 2 — Interne Datenarchitektur

### 2.1 Rohdaten strikt getrennt speichern

**Kernregel:** Niemals direkt auf GUI-Strings arbeiten.

Empfohlene Struktur:

```c
struct AIMessage
{
    STRPTR raw_utf8;
    ULONG  raw_length;

    STRPTR display_text;

    struct MinList codeblocks;
};
```

Codeblock:

```c
struct AICodeBlock
{
    struct MinNode node;

    STRPTR language;

    STRPTR raw_code;
    ULONG  code_length;
};
```

*(Namen und Felder können bei der Implementierung leicht angepasst werden; die Trennung „Rohdaten vs. Anzeige vs. Codeblöcke“ ist verbindlich.)*

---

## Phase 3 — Streaming-System

### 3.1 Netzwerkstream niemals direkt rendern

**Falsch:** `curl_chunk → GUI`

**Richtig:**

```text
curl_chunk
→ stream buffer
→ utf8 validator
→ parser
→ internal message
→ GUI
```

### 3.2 UTF-8 State Machine

UTF-8-Zeichen können über Chunk-Grenzen hinweg zerteilt sein.

```c
struct UTF8StreamBuffer
{
    UBYTE *buffer;
    ULONG size;
    ULONG used;
};
```

Vorgehen:

- neue Chunks anhängen
- UTF-8 validieren
- unvollständige Sequenzen puffern
- erst komplette Zeichen an die nächste Stufe geben

---

## Phase 4 — Minimal-Markdown-Parser

### 4.1 Kein vollständiger Markdown-Parser

Nur erkennen:

````text
```lang
...
```
````

Mehr nicht.

### 4.2 Parser-State-Machine

```c
enum ParserState
{
    PARSER_TEXT,
    PARSER_CODE
};
```

### 4.3 Parserlogik

**TEXT-Modus:** Suche nach öffnendem Fence ` ``` ` (optional Sprache in derselben Zeile). Bei Treffer: Sprache extrahieren, neuen Codeblock anlegen, in **CODE-Modus** wechseln.

**CODE-Modus:** Alles **RAW** übernehmen. Keine Escape-Interpretation, keine `%-`Verarbeitung, keine zusätzliche UTF-8-Konvertierung, keine Zeilenmanipulation — nur speichern, bis schließendes Fence ` ``` `.

---

## Phase 5 — GUI-Architektur

### 5.1 Chatfenster

MUI Text oder einfacher **TextEditor.mcc**:

- Plain Text
- minimale Formatierung
- keine komplexen Styles
- kein Richtext

Nur z. B.:

```text
User:
Assistant:
```

und Hinweise wie:

```text
[Codeblock verfügbar]
```

### 5.2 Codefenster separat

**Scintilla.mcc nur für Code** — zentrale Architekturentscheidung.

---

## Phase 6–10 — Scintilla Code-Viewer v0.1 (ein Implementierungsblock)

Die Kapitel **6–10** sind **ein** Release-Schritt, keine fünf getrennte Meilensteine:

| Kapitel | Rolle |
|---------|--------|
| **6** | Technik: Scintilla im Fenster „View code blocks“ |
| **7** | Regel: Copy aus `raw_code` / `raw_utf8`, nicht Anzeige-Text |
| **8** | String-Sicherheit beim Einbau |
| **9** | Optional: Debug-Logs (nicht Blocker für v0.1) |
| **10** | Definition of Done für den Code-Viewer |

**Aktuell:** [Phase 8 — String-Safety](PHASE-8-STRING-SAFETY.md) **erledigt** (8.1–8.4). [Phase 9 — Debug-Logs](PHASE-9-DEBUG-LOGS.md) **minimal erledigt**. Recovery: R1/R3 erledigt; **R2-Rest** dokumentiert. **Nächster Schritt:** **10** (DoD), **11**, **12** (Chat-Scintilla), **13**. Chat-Ausgabe bleibt NFloattext bis **Phase 12**.

### Umgesetzt (Phase 6, Branch scintilla — auf MorphOS validiert)

- `src/CodeBlocksScintilla.c` — `MUIM_Scintilla_Command` = `MUIA_Scintilla_dummy + 2`, `DoMethodA` + `MUIP_Scintilla_Command` → `SCI_SETTEXT` / `SCI_SETCODEPAGE` (UTF-8), read-only Viewer-Init (Details: [MORPHOS-SDK-NATIV-UND-WSL.md](MORPHOS-SDK-NATIV-UND-WSL.md) §5)
- `gui.c` — Fenster „View code blocks…“ mit **Scintilla** + Scrollgroup statt NFloattext (MorphOS); **OS3/OS4** weiterhin NFloattext + `build_conversation_codeblocks_utf8()` + `CodesetsUTF8ToStr`
- `src/CodeBlocksViewer.c` (ab **7a**) — NList + ein Block pro Zeile in Scintilla (`raw_code`); **7b** Copy/Save; Shutdown `codeBlocksViewerPrepareShutdown()`
- `MUIC_Scintilla` in `MUIA_Application_UsedClasses`; MorphOS: `ttengine.library` öffnen
- Cross-Build: nur SDK-Header, keine zusätzliche `-l` in `Makefile.MorphOS`; Daemon ohne `CodeBlocksViewer.c` / `CodeBlocksScintilla.c`
- **OS3/OS4:** `#ifndef __MORPHOS__` — Code-Viewer bleibt NFloattext + `codesets`; `CodeBlocksScintilla.c` nur in `Makefile.MorphOS`

---

## Phase 6 — Scintilla.mcc Integration (Code-Viewer)

### 6.1 Erste Minimalintegration

Nur:

- `SCI_SETTEXT` / `SCI_GETTEXT`
- `SCI_SETLEXER` (Basis; echtes Highlighting erst in Phase 11)

### 6.2 Initialisierung (Pseudo)

```c
Object *sci = NewObject(
    ScintillaMCC_GetClass(),
    NULL,
    TAG_DONE);
```

*(Konkrete Tags und Klassenname an die tatsächliche Scintilla.mcc-API anpassen.)*

### 6.3 Code setzen

Über **MUI** (nicht direkt `SCI_*` als Top-Level-`DoMethod`):

```c
struct MUIP_Scintilla_Command cmd;
cmd.MethodID = MUIM_Scintilla_Command;  /* MUIA_Scintilla_dummy + 2 */
cmd.iMessage = SCI_SETTEXT;
cmd.wParam = 0;
cmd.lParam = (LONG)raw_code;
DoMethodA(sci, (Msg)&cmd);
```

Vorher `SCI_SETCODEPAGE` mit `SC_CP_UTF8`. **RAW UTF-8** direkt — keine `CodesetsUTF8ToStr` im Code-Viewer.

---

## Phase 7 — Block-Auswahl, Copy, Export (Planung festgehalten 2026-05-22)

### Ist-Zustand nach Phase 7a/7b (MorphOS)

- Fenster „View code blocks…“: **NList** (`index:language`) + **Scintilla** mit **nur** `raw_code` des aktiven Blocks (keine Fences im Viewer).
- **Copy/Save:** vier Buttons; Menü **Bearbeiten → Kopieren** bei geöffnetem Code-Fenster = Copy UTF-8 des aktiven Blocks (nicht Scintilla-Markierung).
- **OS3/OS4 (unverändert):** ein NFloattext mit allen Blöcken (`build_conversation_codeblocks_utf8`, Platzhalter-Format).
- **Font (Anzeige):** DejaVu Sans Mono via TTEngine (in Flow Studio mit UTF-8-Testdatei bestätigt).

### Zielbild (historisch Phase 7 — erreicht)

Ein Nutzer arbeitet mit **einem** Codeblock; Copy/Save liefern bewusst **Rohdaten** oder optional **System-Codeset** — analog zueinander, nicht vom Zufall der Textmarkierung abhängig.

---

### Phase 7a — UI + Copy UTF-8 — **erledigt** (MorphOS, getestet 2026-05)

**Layout (nur `#ifdef __MORPHOS__`; OS3/OS4 vorerst unverändert NFloattext):**

```text
┌──────────┬─────────────────────────────────────┐
│1:python  │  Scintilla (read-only)               │
│2:-       │  nur raw_code des aktiven Blocks     │
│ (NList,  │  UTF-8 + TTEngine — nimmt Restbreite│
│  schmal) │                                      │
└──────────┴─────────────────────────────────────┘
        [ Copy (UTF-8) ] … (vier Buttons)
```

Layout: NList `MUIA_HorizWeight 0` + `MUIA_FixWidth 128`; Scintilla `MUIA_HorizWeight 100` (`gui.c`).

| Element | Spezifikation |
|---------|----------------|
| **NList links** | Ein Eintrag pro `AICodeBlock` der aktuellen `Conversation`, Reihenfolge wie heute (globaler `index`). |
| **Listentext** | `"%lu:%s"` → z. B. `3:python`; leere Sprache → `2:-` (nicht Chat-String `[Codeblock n]`). |
| **Scintilla rechts** | `SCI_SETTEXT` nur mit `block->raw_code` (keine Fences, kein Platzhalter). |
| **Auswahl** | Klick/Liste aktiv → Scintilla neu laden; `AICodeBlock` nur referenzieren (Owner = Conversation). |
| **Copy (UTF-8)** | Button unter Liste+Scintilla; `WriteClipText` mit `MIBENUM_UTF_8` (**Rückgabe 0 = Erfolg**). |
| **Strg+C** | Im Code-Fenster = gleich wie Copy-Button (kein Hauptmenü-Copy). |
| **Menü Ansicht** | „Codeblöcke anzeigen…“ per `MUIA_Menuitem_Enabled` wenn keine Blöcke (kein Fehler-Dialog). |
| **Paste** | Kein Ziel (read-only Viewer). |

**Code:** `CodeBlocksViewer.c`, `gui.c` (Fenster), `menu.c` (`refreshViewCodeBlocksMenuState`). Deploy: nur **LHA** nach Z: (`package-morphos-cross.sh`).

**Entfällt in 7a:** `build_conversation_codeblocks_utf8()` für die MorphOS-Anzeige (bleibt für OS3/OS4).

**Nicht Teil von 7a:** Mausrad-Scroll im **Haupt-Chat** (NFloattext) — siehe **Phase 7c** (älteres Verhalten, Scrollbar funktioniert).

---

### Phase 7b — Save + Copy System-Codeset — **erledigt** (MorphOS, getestet 2026-05)

**Gemeinsame Konvertierung** (`CodeBlocksViewer.c` — `codeBlocksPayloadFromBlock`):

| Modus | Bytes | Verwendung |
|-------|--------|------------|
| **Raw** | `raw_code` / `code_length` | Copy UTF-8, Save UTF-8 |
| **System** | `CodesetsUTF8ToStr(CSA_DestCodeset, systemCodeset, raw_code, …)` | Copy System, Save System; Free mit `CodesetsFreeA` |

**UI (MorphOS Code-Fenster):** vier Buttons unter Liste+Scintilla:

| Button | Wirkung |
|--------|---------|
| Copy (UTF-8) | 7a |
| Copy (system charset) | Zwischenablage, `systemCodeset->MIBenum` |
| Save (UTF-8)… | ASL Save, Raw-Bytes |
| Save (system charset)… | ASL Save, konvertiert |

Button-Beschriftung **ohne** „Code block“ (Kontext: Code-Blöcke-Fenster). ASL-Titel bleibt **Save code block** / **Codeblock speichern**.

**Save:** ASL mit Parent **`mainWindow`** (`codeBlocksViewerSetAslParentWindow` in `MainWindow.c`) — nicht das Code-Blocks-MUI-Subfenster (vermeidet Freeze auf MorphOS). Save **deferred** per `MUIM_Application_PushMethod`; beim Button-Klick **Snapshot** des `AICodeBlock*`. Schreiben per **`Open`/`Write`/`Close`** (nicht `stdio`). Default-Dateiname `block-<index>.<ext>`; Drawer-Default `SYS:`; ASL-Abbruch = kein Fehlerdialog.

**Shutdown:** `codeBlocksViewerPrepareShutdown()` in `shutdownGUI()` vor `MUI_DisposeObject(app)` — Cache leeren, Scintilla leeren, Fenster schließen (vermeidet UAF auf `AICodeBlock` in der Conversation).

**Hinweise:**

- **UTF-8 Raw:** IDE, WSL, Git, moderne Editoren — **empfohlen**.
- **System-Codeset:** alte Amiga-Editoren, manche MUI-Felder; Emoji/Sonderzeichen können fehlen oder `?` werden (wie im Chat mit NFloattext).
- Copy (UTF-8) und Save (UTF-8) liefern dieselben Bytes; nur Ziel unterscheidet sich.

---

### Abgrenzung Chat

| Ort | Verhalten (MorphOS) |
|-----|---------------------|
| **Code-Viewer** | 7a/7b: aktiver Block, Raw oder System-Codeset (Buttons + Bearbeiten→Kopieren) |
| **Chat (NFloattext)** | Anzeige/`display_text`, codesets — unverändert bis **Phase 12**; Stream-Sync: [STREAM-RECOVERY.md](STREAM-RECOVERY.md) |

---

### Implementierungsreihenfolge

1. ~~**7a**~~ — erledigt (NList, Scintilla, Copy-Button, Menü-Ghosting, Speicher-Dismiss).
2. ~~**7b**~~ — erledigt (Copy/Save UTF-8 + System, ASL, vier Buttons).
3. **7c** — Mausrad → Chat-NListview/NFloattext scrollen ✓.
4. **Phase 8** — Buffer/`snprintf` beim Umbau prüfen (100 KB-Grenze, dynamische Pfade).
5. **Phase 9** — optional Logs.
6. **Phase 10** — DoD-Checkliste abhaken.

**Katalog:** neue Menü-Strings → `AmigaGPT.pot` / `.po`, FlexCat beim Build.

Paste im read-only Code-Viewer: weiterhin **kein** Ziel.

---

### Phase 7c — Chat-Ausgabe: Mausrad-Scroll ✓ (2026-05)

**Shutdown:** `chatOutputWheelShutdown()` nur Event-Handler entfernen; `chatOutputWheelDisposeClass()` **nach** `MUI_DisposeObject(app)` — sonst Privilege Violation (Custom-Class gelöscht, NFloattext-Instanz noch aktiv).

**Umgesetzt:** `MainWindow.c` — Fenster-Event-Handler (`IDCMP_RAWKEY`, NewMouse `NM_WHEEL_*` / MUIKEY_UP|DOWN) scrollt `chatOutputListView` per `MUIA_NList_First` (`Up`/`Down`, mit Shift `PageUp`/`PageDown`), nur wenn der Mauszeiger über der Chat-Liste liegt.

**Abgrenzung:** **Phase 12** ersetzt die Chat-**Ausgabe** durch Scintilla; 7c betrifft bis dahin das NFloattext-Setup.

**Priorität:** nach **7b** oder parallel, kleiner Scope (ein Notify-Hook, kein Datenmodell).

---

## Phase 8 — String- und Sicherheitsdisziplin

**In Arbeit (2026-05):** [PHASE-8-STRING-SAFETY.md](PHASE-8-STRING-SAFETY.md) — Audit, R2-Rest-Verweis, erster Fix-Sweep (8.1).

**Hinweis:** [Stream- & Chat-Recovery (R1–R4)](STREAM-RECOVERY.md) ist **kein** Unterpunkt von Phase 8. **R2** nur teilweise erledigt (R2.2); Rest siehe Phase-8-Doc.

**Vermeiden:**

- `sprintf(buf, text);` / `printf(text);` mit unbekanntem Inhalt
- feste Mini-Buffer (`char buf[1024];`) für unbegrenzte Modell-Ausgaben

**Stattdessen:**

- explizite Formatstrings: `snprintf(buf, size, "%s", text);`
- dynamische Allokation oder dokumentierte Obergrenzen mit sauberem Fehlerpfad

---

## Phase 9 — Debugging-Infrastruktur

**Erledigt (minimal):** [PHASE-9-DEBUG-LOGS.md](PHASE-9-DEBUG-LOGS.md) — `T:amigagpt_stream.log`, optional `T:amigagpt_utf8.log`, Config `debugStreamLog`, `KPrintF` → LogTool.

**Optional später:** Raw-Stream-Log (`T:amigagpt_raw.log`), erweiterte UTF-8-Parser-Zeilen.

---

## Phase 10 — Erste Version (0.1) — Definition of Done für 6–10

**Enthalten (Code-Viewer, MorphOS) — größtenteils erledigt (6, 7a, 7b):**

- Menü **View code blocks…** mit **NList** (`index:language`) + **Scintilla** (ein Block, `raw_code`, UTF-8, TTEngine, DejaVu Sans Mono) — **erledigt**
- **Copy/Save UTF-8** und **Copy/Save System-Codeset** aus aktivem Block — **erledigt (7b)**
- Bearbeiten→Kopieren bei offenem Code-Fenster = Copy UTF-8 des aktiven Blocks — **erledigt**
- Codeblock-Erkennung (Fences) unverändert in `addTextToConversation()`; Chat-Platzhalter unverändert
- **Offen in 8–10:** String-Safety-Sweep, optionale Logs, formale DoD-Abhakung

**Ausgeschlossen in 6–10:**

- Syntax-Highlighting (Phase 11)
- Chat-Hauptfenster auf Scintilla (Phase 12)
- Chat-Eingabe-Editor umstellen
- Cairo
- Themes / HTML / vollständiges Markdown

---

## Phase 11 — Komfort (erst nach Stabilität)

- **Syntax Highlighting:** `SCI_SETLEXER` für C, Python, JSON, Shell, Lua, …
- **Inline-Code** (optional, oft entbehrlich)
- **Tabs** für mehrere Codeblöcke (Alternative zur NList aus 7a)
- **Lexer** (siehe oben); Save markdown o. Ä. nur falls zusätzlich gewünscht — Copy/Save Raw/Codeset in **7b**

---

## Phase 12 — Hauptfenster Chat-Ausgabe → Scintilla

**Ziel:** Unicode-Anzeige im Chat ohne `CodesetsUTF8ToStr` → NFloattext (Motivation: MorphOS-Prüfung Emoji → `??`, siehe [UNICODE-MORPHOS-MUI.md](UNICODE-MORPHOS-MUI.md)).

**Enthalten:**

- Große **Chat-Ausgabe** im Hauptfenster: NFloattext durch **read-only Scintilla** ersetzen
- `conversationNodeGetDisplay()` / Stream: UTF-8 direkt an Scintilla (`SCI_SETTEXT` / `SCI_APPENDTEXT` o. Ä.)
- Rendering: **TTEngine** (wie Code-Viewer aus 6–10)
- Markdown: Scintilla-Styles oder Plain (Fortführung `config.markdownFormatting`)

**Ausgeschlossen:**

- Chat-**Eingabe** (`TextEditor` / `AmigaGPTTextEditor`) — eigenes Thema
- NList-Titel (`name_list_display` + codesets) — bleibt
- Code-Viewer-Fenster — bleibt separates Fenster (oder gleiches Muster wie 6–10)
- Cairo

**Abhängigkeit:** Phase 6–10 und 11 abgeschlossen oder stabil (Scintilla+TTEngine im Projekt bewährt).

---

## Phase 13 — MorphOS-spezifisch (Worker / UI-Batching)

*Bisherige „Phase 12“ — bewusst **nach** Phase 12, nicht davor (Worker soll den Scintilla-Chat-Stream optimieren, nicht NFloattext nachrüsten).*

- **MUI Worker-Task** für Streaming — UI nicht blockieren
- **Signal/Event-Modell:** `network task → parser task → ui task`
- **Keine GUI-Updates pro Zeichen** — sammeln, batchweise aktualisieren (Lag, Repaint, CPU)

---

## Aufwand (Orientierung)

| Bereich                 | Schwierigkeit     |
| ----------------------- | ----------------- |
| libcurl / OpenAI        | leicht            |
| Streaming               | mittel           |
| UTF-8 korrekt           | mittel–schwer     |
| Markdown-Minimalparser  | leicht            |
| MUI Chatfenster         | leicht            |
| Scintilla-Integration   | leicht            |
| Copy/Paste korrekt      | mittel            |
| Full Markdown           | sehr schwer       |

**Grobe Spanne für eine stabile Minimalversion:** oft **2–6 Wochen** für eine erfahrene MorphOS/C-Person — abhängig vom Refaktor-Umfang im bestehenden AmigaGPT-Code.

---

## Leitlinie

**„AI-Terminal“ statt „Discord-Klon“:** robuste Textausgabe, verlässliche Codeblöcke, stabile UTF-8-Pipeline, minimale Formatierung — passt zu Amiga/MUI und reduziert Risiko.

---

## Anknüpfung an den aktuellen Code (Stand Fork)

- Nachrichten: `struct ConversationNode` / `struct Conversation` in `openai.h` — Ausgangspunkt für Migration zu getrennten Roh-/Anzeige-/Codeblock-Daten.
- Streaming: Logik in `openai.c` und Anbindung in `MainWindow.c` — Ziel ist die oben beschriebene Pipeline zwischen Netz und UI.
- Chat-UI: `MainWindow.c` nutzt u. a. NList/TextEditor und Rich-Text-Hilfen — für v0.1 bewusst vereinfachen oder parallel neu aufsetzen, je nach Refaktor-Strategie.

### Umgesetzt (Branch scintilla)

**Phase 3.2**

- `src/utf8stream.c` / `utf8stream.h` — `UTF8StreamBuffer`, nur vollständige UTF-8-Sequenzen werden weitergereicht.
- `sendChatMessage()` in `MainWindow.c`: SSE-Chunks → `utf8stream` → `receivedMessage` / Live-Anzeige; `utf8stream_flush()` am Stream-Ende.
- **Hinweis:** Während des **Live-Streams** baut die UI den laufenden Text noch aus dem Rohpuffer; **`display_text`** / Fence-Platzhalter (Phase 5.1) entstehen erst beim Abschluss der Antwort in **`addTextToConversation()`** und greifen sichtbar, wenn der Chat aus den gespeicherten Knoten neu gezeichnet wird (`displayConversation()`).
- Host-Tests: `tools/test-utf8stream.sh`

**Phase 2 (Datenmodell, Verhalten unverändert)**

- `struct AICodeBlock` + `struct ConversationNode` mit `raw_utf8`, `raw_length`, `display_text`, `codeblocks` (`openai.h`).
- `conversationNodeGetRaw()` / `conversationNodeGetDisplay()` (`gui.c`).
- History/API nutzen `raw_utf8`; Anzeige nutzt `display_text` falls gesetzt, sonst `raw_utf8`.

**Phase 4 (Minimal-Fence-Parser)**

- `src/codefence.c` / `codefence.h` — erkennt Zeilen mit öffnendem ` ``` ` (optional Sprache in derselben Zeile) und schließendem ` ``` ` am Zeilenanfang; füllt `codeblocks` mit `struct AICodeBlock` (`language`, `raw_code`, `code_length`). Unvollständiges schließendes Fence am Textende wird ignoriert.
- Aufruf aus `addTextToConversation()` in `gui.c` — jede neue Nachricht (inkl. Import/Kopie) wird geparst.

**Phase 5.1 (Chat vs. Code, ohne Scintilla)**

- Wenn mindestens ein vollständiger Fence-Block erkannt wurde: `display_text` enthält den Fließtext mit jedem Block ersetzt durch den **katalogisierten** Platzhalter `STRING_CHAT_CODEBLOCK_PLACEHOLDER` (Standard: `[Codeblock]\n` in `AmigaGPT.pot`; Übersetzungen in den `.po`-Dateien). Ohne vollständige Fences bleibt `display_text` NULL — Anzeige wie bisher über `raw_utf8`.
- Konversationstitel in der **linken NList**: kein natives UTF-8 in `NList_mcc.h`; `name` bleibt UTF-8 (JSON/API), Anzeige über **`name_list_display`** (`CodesetsUTF8ToStr`, `conversationRefreshNameListDisplay()` in `gui.c`).

**Phase 5.2 (separates Codefenster — historisch NFloattext)**

- Menü **Ansicht → „View code blocks…“**: Inhalt = **alle** `codeblocks` der aktuellen Unterhaltung; Platzhalter-Nummer (`AICodeBlock.index`) **gesamt** über den Chat. **`getCurrentConversation()`** (`MainWindow.c`).

**Phase 6 (MorphOS):** dasselbe Fenster mit **Scintilla** statt NFloattext — siehe oben „Umgesetzt (Phase 6)“.

**Nächster Schritt (Empfehlung):** **Stream-Recovery R1+R2** ([STREAM-RECOVERY.md](STREAM-RECOVERY.md)), optional parallel **7c** (Mausrad). Danach R3, **Phase 8**, 9–10, Phase 11, **Phase 12**, **Phase 13**.

---

## Optionales Aufräumen (nicht dringend)

- **`MainWindow.c` / `MainWindow.h`:** Der Zeiger auf die Chat-**Ausgabe** heißt noch `chatOutputTextEditor`, ist aber ein **`NFloattextObject`** (`NFloattext.mcc`), kein `TextEditor`. Umbenennung z. B. in `chatOutputFloattext` (und ggf. `chatOutputTextEditorContents` → `chatOutputFloattextContents`) würde die Lesbarkeit verbessern — reiner Refactor, viele Treffer, **keine** Verhaltensänderung.

- **Mini-Markdown schlauer machen** (nach abschaltbarer Option, siehe unten umgesetzt): `\n` in `\n`-Sequenzen nicht als Escape werten; `*`/`_` nur in echten `**`/`*`-/`__`-Markern; Markdown-Lauf beim Streaming **nur** auf Assistant-Anteil, nicht User+Assistant gemeinsam; Formatierung außerhalb von Code-Platzhaltern. Eigene Parser-/Puffer-Schritte, Speicher auf MorphOS vorsichtig (AllocVec/FreeVec, keine Doppel-Frees).

### Umgesetzt: Mini-Markdown abschaltbar

- Menü **Ansicht → Markdown formatting** (`config.markdownFormatting`, Default **an**), persistiert in `config.json`.
- Aus: Assistant-Text unverändert (UTF-8-Kopie) → Codesets → NFloattext; kein `convertMarkdownFormattingToMUI`.
- Umschalten: `displayConversation(NULL)` (kein Fenster-Neubau).
