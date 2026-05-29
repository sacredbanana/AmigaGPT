# Plan: Stream- & Chat-Recovery (MorphOS / OpenAI-Chat)

Eigenständiger Planabschnitt für den Branch **scintilla**. Ergänzt [SCINTILLA-ARCHITECTURE.md](SCINTILLA-ARCHITECTURE.md) (Code-Viewer, Phasen 6–13) und [UNICODE-MORPHOS-MUI.md](UNICODE-MORPHOS-MUI.md) (Zeichensatz Chat vs. Code).

**Ziel:** Aus typischen Fehlerszenarien beim **Chat-Stream** und der **Chat-Anzeige** kontrolliert wieder in einen konsistenten Zustand kommen — ohne Panik-Dialoge, wenn Daten schon da sind, und ohne „Zombie-UI“ (Busy an, Buttons aus, Chat zeigt veralteten Stream-Puffer).

**Nicht in diesem Plan:** Bild-API, Daemon, Rexx, History-JSON, Proxy-Setup, Code-Viewer Copy/Save (7a/7b erledigt), Phase 12 Chat-Scintilla.

**Abgrenzung zu Phase 8:** **R1–R4 sind kein Unterpunkt von Phase 8**, sondern ein **eigener, priorisierter Parallel-Track** (Stream-Ende, UI-Sync, Transport). **Phase 8** bleibt die technische String-/Buffer-Safety-Schicht (`snprintf`, Grenzen, saubere Fehlerpfade). Beim Umsetzen von R1/R2 die Phase-8-Regeln mitziehen — Verhalten zuerst (R1/R2), Safety im gleichen Diff wo nötig.

---

## Roadmap (Recovery)

| Phase | Inhalt | Priorität |
|-------|--------|-----------|
| **R1** | Einheitliches Stream-Ende + UI-Sync (Chat = Modell) | hoch — **umgesetzt** (2026-05) |
| **R2** | Transport: WANT_READ / partieller Stream weich beenden | hoch — **erledigt** (2026-05-25) |
| **R3** | Stream-UI-Last (Freeze reduzieren) | mittel — **umgesetzt** (2026-05) |
| **R4** | Fence-Heuristik + Logs (optional) | niedrig |

**Reihenfolge zur Umsetzung:** **R1 → R2 → R3** (R4 bei Bedarf). Parallel möglich: **7c** (Mausrad Chat) — klein, kein Überschneidungskonflikt.

---

## Referenzfall (MorphOS-Test 2026-05)

Ausführlicher Chat mit Bitte um **Hello World in C mit kyrillischen Zeichen**:

| Beobachtung | Einordnung |
|-------------|------------|
| App einige Sekunden „eingefroren“ beim Empfang | **R3** — Stream-UI (`CodesetsUTF8ToStr`, NFloattext-Refresh, ggf. Speech auf vollem Puffer) |
| Text im Chat sichtbar, ```-Zäune noch sichtbar | **R1** — Stream-Puffer ≠ `display_text` mit Platzhaltern |
| Dialog „SSL: timeout … (WANT_READ)“ | **R2** — Transport als Fehler, obwohl Inhalt teils schon da |
| Code-Blocks-Fenster: Hello World / Kyrillisch da, Copy UTF-8 → Flow Studio OK | **Daten OK** — `raw_code` / Parser / 7a–7b funktionieren |
| „Neue Codestellen nicht zu Blöcken“ (subjektiv) | oft **unvollständige Fences** oder Chat zeigt noch Roh-Stream — Viewer zeigt trotzdem geparste Blöcke |

Dieser Fall ist die **DoD-Referenz** für R1+R2.

**Shutdown (2026-05):** Beenden mit offenem Code-Viewer konnte MorphOS instabil machen (UAF: Cache auf `AICodeBlock` nach `MUI_DisposeObject`). **Fix (Code, Commit `1048e16`):** `codeBlocksViewerPrepareShutdown()` in `shutdownGUI()` vor `MUI_DisposeObject` — auf MorphOS **getestet** (Build 8552 / 2.17.70). **Zusätzlich:** `RAM:` ist **systemkritisch** (ENV) — kein Pattern-`Delete` in AmigaGPT; Absturz kann von instabilem System/ENV kommen, nicht nur von Save einer Datei nach `RAM:`.

### MorphOS: `RAM:` und ENV (systemkritisch)

Auf MorphOS liegen u. a. **ENV-Variablen** unter **`RAM:`** (Laufzeit-ENV; Persistenz über **`ENVARC:`**). **`RAM:`** ist **nicht** „beliebiger Scratch-Speicher“ wie ein Windows-Temp-Ordner — **`Delete("RAM:#?")`** oder das Löschen/Korumpieren zentraler ENV-Inhalte kann das System **genauso destabilisieren** wie das Zerstören der Windows-Registry.

| Aktion | Risiko |
|--------|--------|
| **Eine Datei** nach `RAM:meincode.c` schreiben (Codeblock-Save) | unkritisch — normales `Open`/`Write`, eine Datei |
| **`Delete("RAM:#?")`**, RAM leeren, falsche Wildcards | **hoch** — ENV/System kann mitsterben |
| System schon instabil (WANT_READ, Freeze, Speicher) + Quit | Absturz kann **Folge** sein, nicht Ursache |

**AmigaGPT Codeblock-Save (7b):** nur **`Open(MODE_NEWFILE)`** + **`Write`** auf den vom Nutzer gewählten **Einzelpfad** — **kein** `Delete("RAM:…")`, kein Wildcard-Löschen.

**Audit: Keine Pattern-Aufräumaktion auf `RAM:`** (Stand Code-Review 2026-05)

Im gesamten Repo (`src/*.c`, Rexx unter `bundle/`) gibt es **kein** `Delete("RAM:#?")`, **kein** `DeleteFiles`, **kein** Wildcard-Löschen auf `RAM:`.

| Ort | Aufruf | Wann |
|-----|--------|------|
| `CodeBlocksViewer.c` | `Open`/`Write`/`Close` | Save nur **vom Nutzer gewählter Einzelpfad** |
| `MainWindow.c` | `Delete(entry->filePath)` | Bild aus Liste entfernen — **ein** Pfad aus `filePath` (kann `RAM:…` sein, wenn Bild dort liegt) |
| `MainWindow.c` | `Delete("AMIGAGPT:chat-history.json")` | Nur bei **kaputtem JSON** nach Laden |
| `MainWindow.c` | `copyFile(…, "RAM:chat-history.json")` | **Kopie eine Datei** ins RAM (kein Löschen) |
| `menu.c` | `Delete("ENVARC:mui/…")`, `Delete("ENV:mui/…")` | Menü „MUI-Einstellungen löschen“ — **feste Pfade**, nicht `RAM:` |
| `MainWindow.c` | `Open`/`Write` `ENVARC:AmigaGPT/last-conversation` | Zuletzt gewählter Chat-Name (persistent, kein `RAM:`) |
| `config.c` | `Open`/`Write` `ENVARC:AmigaGPT/config.json` | Einstellungen/API-Keys (Fallback-Lesen `AMIGAGPT:config.json`) |
| `speech.c` | `Delete(output)` dann `Open(output, …)` | TTS: **eine** Ausgabedatei (oft `T:…`, nur wenn `output` gesetzt) |
| Rexx `say.rexx` | `Delete T:AmigaGPTInput` | nur **`T:`**, nicht `RAM:` |

**Fazit zur Befürchtung:** Ein Quit-Absturz passt **nicht** zu einer versteckten „RAM:-Aufräumaktion per Pattern“ in AmigaGPT — die gibt es im Code nicht. Möglich bleiben: **Shutdown-UAF** (Code-Viewer), **manuell** `Delete RAM:#?` in der Shell, oder **System schon instabil**.

**Test-Hinweis:** Save nach `RAM:block-N.c` ist OK; **RAM: nicht pauschal löschen** (Shell/Utilities).

---

## Drei Schichten (Recovery-Modell)

```text
┌─────────────────────────────────────────────────────────┐
│ 3. UI        Buttons, Busy, NFloattext = Conversation   │
├─────────────────────────────────────────────────────────┤
│ 2. Daten     receivedMessage → Node → Fences → display  │
├─────────────────────────────────────────────────────────┤
│ 1. Transport SSL/Socket, SSE, response.completed        │
└─────────────────────────────────────────────────────────┘
```

**Regel:** Recovery von **unten nach oben** abschließen — erst Transport-Zustand klären, dann Daten persistieren/parsen, dann UI aus dem Modell neu aufbauen.

| Schicht | Ist (vereinfacht) | Soll |
|---------|-------------------|------|
| **1. Transport** | `displayError("SSL: … WANT_READ")`, `chatStreamInProgress = FALSE`, Abbruch | Bei bereits empfangenem Assistant-Text: **kein** harter Fehler; Status „Antwort unvollständig“ (Katalog) |
| **2. Daten** | `addTextToConversation` am Ende; Fences nur bei **geschlossenem** ` ``` ` | Immer parse nach Assistant-Node; `display_text` mit Platzhaltern wenn ≥1 Block |
| **3. UI** | Live-Stream in `chatOutputTextEditorContents`; `displayConversation` am Ende, kann ausbleiben/fehlschlagen | **Immer** nach gespeichertem Assistant: `displayConversation`; Busy aus, Buttons an |

---

## Szenario-Katalog

### S1 — Partieller Stream (WANT_READ, Timeout, Verbindung zu)

- **Trigger:** `openai.c` — `sslWaitForReadReady` scheitert oder `bytesRead == 0` im Stream-Zweig; ggf. kein `response.completed`.
- **Heute:** Fehlerdialog; `receivedMessage` kann voll sein; Loop in `sendChatMessage` endet; oft `addTextToConversation` + teils kein sauberer Chat-Rebuild.
- **Soll (R2+R1):** Wenn `receivedMessage[0] != '\0'`: Stream **partial** beenden → Assistant speichern → **R1** UI-Sync → **optional** Info-Status (kein roter SSL-Rohstring).

### S2 — Chat zeigt ```, Code-Viewer zeigt Blöcke

- **Trigger:** Während Stream nur Roh-UTF-8 in NFloattext; nach Ende fehlt oder scheitert `displayConversation`; Nutzer sieht alten Puffer.
- **Heute:** Modell hat `codeblocks` + `display_text`, Chat noch Stream-Ansicht.
- **Soll (R1):** Nach jedem erfolgreichen `addTextToConversation(..., "assistant")` zwingend `displayConversation(currentConversation)`; `refreshViewCodeBlocksMenuState()`.

### S3 — UI-Freeze während Stream

- **Trigger:** `appendAssistantStreamText` — pro Chunk UI-Refresh; Speech: Konvertierung **gesamtes** `receivedMessage` (OS3/4 NFloattext; MorphOS Scintilla).
- **Soll (R3):** Speech nur auf neuem `piece`; seltener Refresh; `MUIM_Application_NewInput` beibehalten; kein `displayConversation` während Stream (bereits so — beibehalten).
- **MorphOS (2026-05):** Live-Stream nutzt `chatOutputScintillaAppendStreamDelta()` (`SCI_APPENDTEXT` + Styling nur für neues Tail) statt vollem `SCI_SETTEXT` pro Refresh; nach Stream-Ende `displayConversation` wie bisher.

### S4 — 64-KB-Stream-Puffer voll

- **Trigger:** `openai.c` — „Stream read buffer full (64 KB)“.
- **Soll:** Wie S1 (partial) + Katalog-String; Truncation in `receivedMessage` dokumentieren.

### S5 — Harte API-Fehler (JSON `error`, kein Inhalt)

- **Trigger:** `sendChatMessage` — Error-Zweig entfernt letzte User-Nachricht.
- **Soll:** Policy **einmal** festhalten (User-Nachricht behalten vs. zurück); überall gleich; Buttons/Busy in **einem** Pfad.

### S6 — Unvollständige Code-Fences

- **Trigger:** `codefence.c` — schließendes ` ``` ` am Zeilenanfang fehlt (abgeschnittener Stream, Modell vergisst Fence).
- **Heute:** Kein Block / kein `display_text`; Roh-``` im Chat.
- **Soll (R4, optional):** Unverändert dokumentieren **oder** Heuristik „offenes Fence bis EOF“ als ein Block (bewusst riskant).

### S6b — Eingerückte Fences (Chat zeigt ```, Viewer hat Blöcke)

- **Trigger:** Modell liefert ` ``` ` mit führenden Leerzeichen/Tabs (z. B. in Listen); Parser verlangte bisher Zeilenanfang **ohne** Einrückung.
- **Fix (2026-05):** `codefence.c` — CommonMark-konform bis **3 Spalten** Einrückung vor öffnendem/schließendem Fence.
- **Grenze:** Einrückung **> 3** Spalten bleibt Roh-``` im Chat (kein Block).

### S7 — `displayConversation` bricht ab (Puffer zu groß)

- **Trigger:** `STRING_ERROR_CONVERSATION_MAX_LENGTH_EXCEEDED` — früher Return, NFloattext unverändert.
- **Soll (R1):** Fehler zeigen **und** trotzdem letzte Nachricht / Code-Viewer nutzbar; ggf. nur letzte Assistant-Nachricht rendern (später).

---

## Zentrale API (Zielbild R1)

Eine Funktion in `MainWindow.c` (Name Vorschlag):

```c
enum ChatStreamOutcome {
    CHAT_STREAM_OK,           /* response.completed o.ä. */
    CHAT_STREAM_PARTIAL,      /* Daten da, Transport abgebrochen */
    CHAT_STREAM_FAILED        /* nichts Brauchbares */
};

void finishChatStream(enum ChatStreamOutcome outcome,
                      STRPTR receivedMessage, ...);
```

**Pflichten von `finishChatStream`:**

1. `utf8stream` flushen/freigeben (Caller oder hier)
2. Bei OK/PARTIAL und `receivedMessage` nicht leer: `addTextToConversation` + `conversationNodeParseCodeFences` (bereits in `addTextToConversation`)
3. **`displayConversation(currentConversation)`** — Chat = `display_text` / Platzhalter
4. `refreshViewCodeBlocksMenuState()` (MorphOS)
5. Loading aus, Send/New/Delete-Buttons an, Statusleiste (Katalog)
6. PARTIAL: `updateStatusBar(STRING_CHAT_RESPONSE_PARTIAL, …)` — **kein** `displayError` mit SSL-Rohtext
7. FAILED: bestehende Fehlerpfade; ggf. User-Node zurück

`sendChatMessage` ersetzt verstreute Cleanup-Blöcke durch Aufrufe von `finishChatStream`.

---

## Phase R1 — UI-Sync & einheitliches Stream-Ende

**Dateien:** `MainWindow.c`, `MainWindow.h`, ggf. `openai.h` (Outcome-Flag), Katalog.

| # | Aufgabe |
|---|---------|
| R1.1 | `finishChatStream` einführen; alle Exit-Pfade aus `sendChatMessage` darüber |
| R1.2 | Nach Assistant-`addTextToConversation` **immer** `displayConversation` (auch bei vorherigem Stream-Puffer-Inhalt) |
| R1.3 | S2-Regressionstest: Chat zeigt `[Codeblock n]` (bzw. Katalog-Platzhalter), nicht ``` |
| R1.4 | Katalog: `STRING_CHAT_RESPONSE_PARTIAL`, ggf. `STRING_CHAT_STREAM_TRUNCATED` |

**DoD R1:** Referenzfall — nach Stream (auch mit WANT_READ wenn Text da): Chat-Platzhalter, Code-Viewer unverändert nutzbar, Buttons wieder klickbar.

---

## Phase R2 — Transport weich beenden

**Dateien:** `openai.c`, `MainWindow.c`.

| # | Aufgabe |
|---|---------|
| R2.1 | `postChatMessageToOpenAI` / Stream-Loop: Outcome **PARTIAL** vs **FAILED** unterscheiden (z. B. `receivedBytes` / `readBuffer` / Flag zurückgeben) |
| R2.2 | WANT_READ-Timeout: **nur** `displayError`, wenn kein Assistant-Inhalt im aktuellen Lauf |
| R2.3 | Prüfen: `doneReading = TRUE` bei `bytesRead > 0` ohne `SSL_pending` und ohne `response.completed` — bewusst dokumentieren oder weiterlesen bis Stall/Timeout (verhindert zu frühes Ende) |
| R2.4 | `finishChatStream(PARTIAL)` aus `sendChatMessage` bei Rückgabe „partial“ |

**DoD R2:** Simulierter Abbruch / langsames Netz — kein SSL-Dialog wenn Antworttext schon im Chat-Modell; Status „unvollständig“.

### R2 — Stand (2026-05-25)

| ID | Status | Kurz |
|----|--------|------|
| R2.1 | **erledigt** | `ChatTransportOutcome` + `openAIChatStreamTransportOutcome()` nach Stream-Read |
| R2.2 | **erledigt** | WANT_READ: kein `displayError` bei vorhandenen Daten |
| R2.3 | **erledigt** | Batch-Yield: `doneReading` bei Daten ohne `response.completed`, `chatStreamInProgress` bleibt TRUE; Stall nur bei `bytesRead==0` |
| R2.4 | **erledigt** | `chatStreamClassifyOutcome()` nutzt Transport-Outcome → `finishChatStream(PARTIAL\|FAILED)` |

Details: [PHASE-8-STRING-SAFETY.md](PHASE-8-STRING-SAFETY.md#recovery-r2--rest-bewusst-zurückgestellt).

---

## Phase R3 — Stream-UI-Last

**Dateien:** `MainWindow.c`.

| # | Aufgabe |
|---|---------|
| R3.1 | Speech (non-OpenAI): nicht jedes Mal `CodesetsUTF8ToStr` auf volles `receivedMessage` — nur `piece` oder Index inkrementell |
| R3.2 | `STREAM_UI_REFRESH_CHUNK_INTERVAL` ggf. erhöhen oder zeitbasiert (z. B. max. 5×/s) |
| R3.3 | Optional: während Stream nur ASCII-Zwischenstatus in Statusleiste, kein Voll-Chat-Rebuild |

**DoD R3:** Lange Antwort mit UTF-8 (Kyrillisch) — spürbar weniger Freeze; Endzustand weiterhin R1.

**Umsetzung (2026-05):** `MainWindow.c` — NFloattext-Refresh max. ~5/s (`gettimeofday`, 200 ms); System-TTS nur noch `CodesetsUTF8ToStr` auf UTF-8-Tail statt vollem `receivedMessage`; finaler `streamUiFlushChatDisplay()` vor `finishChatStream`.

---

## Phase R4 — Fences & Logs (optional)

**Dateien:** `codefence.c`, `openai.c` / neues Log-Hook.

| # | Aufgabe |
|---|---------|
| R4.1 | Dokumentierte Entscheidung: unvollständiges Fence am Ende → kein Block **oder** ein Block bis EOF |
| R4.2 | Phase-9-Log: `T:amigagpt_stream.log` — letzte SSE-Zeile, Outcome, Länge `receivedMessage` |
| R4.3 | Host-Test: Fence-Parser mit abgeschnittenem Closing (bereits `codefence` / ggf. erweitern) |

---

## Abgrenzung zu anderen Phasen

| Thema | Wo |
|-------|-----|
| Mausrad Chat | **7c** — UX, kein Recovery |
| `snprintf`/Buffer-Grenzen | **Phase 8** — überlappt S7; gemeinsam angehen |
| Debug-Logs | **Phase 9** — R4.2 |
| DoD Code-Viewer | **Phase 10** ✓ — [PHASE-10-DOD.md](PHASE-10-DOD.md); Recovery blockiert v0.1 nicht |
| Chat UTF-8 ohne codesets (MorphOS) | **Phase 12** — Scintilla-Anzeige, Stream-Puffer UTF-8; TTS weiter codesets — [PHASE-12-CHAT-SCINTILLA.md](PHASE-12-CHAT-SCINTILLA.md) |
| Worker / UI-Batching | **Phase 13** — zurückgestellt; nur wenn R3 nicht reicht |

---

## Testplan (MorphOS)

1. **Happy path:** kurze Antwort mit ` ```c ` … ` ``` ` → Chat Platzhalter, Viewer, Copy UTF-8.
2. **Referenzfall:** C + Kyrillisch, längere Antwort → kein dauerhaftes Freeze (R3), am Ende Chat-Sync (R1).
3. **WANT_READ:** (schwer reproduzierbar) — wenn Text da: kein SSL-Error-Dialog, Status partial (R2).
4. **Abbrechen:** neuer Chat während Busy — keine hängenden Buttons (R1).
5. **Code-Viewer:** nach partial Stream Menü „Codeblöcke“ aktiv wenn Blöcke existieren.

---

## Implementierungsreihenfolge (gesamt)

1. ~~7a / 7b~~ — Code-Viewer Copy/Save
2. **R1** — `finishChatStream` + Chat-Sync
3. **R2** — WANT_READ / partial
4. **7c** — Mausrad (optional parallel)
5. **R3** — Freeze
6. ~~**8–10**~~ — Strings, Logs, DoD ✓ ([PHASE-10-DOD.md](PHASE-10-DOD.md))
7. **R4** — optional

---

## Offene Entscheidungen (vor R2/R1.1 klären)

1. **User-Nachricht bei API-Fehler:** behalten (heute teils entfernt) oder in Eingabefeld zurück?
2. **PARTIAL im Verlauf:** Assistant-Node immer speichern, auch ohne `response.completed`?
3. **SSL-Fehlertext:** nur noch Katalog + Log, nie Roh-`displayError` für WANT_READ wenn partial?

---

*Stand: 2026-05-24 — R1/R3 erledigt; R2 teilweise (Rest dokumentiert); R4 optional zurückgestellt; Phase 8 gestartet ([PHASE-8-STRING-SAFETY.md](PHASE-8-STRING-SAFETY.md)).*
