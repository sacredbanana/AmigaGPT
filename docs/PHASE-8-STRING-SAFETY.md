# Phase 8 — String- und Sicherheitsdisziplin

Branch **scintilla**, MorphOS-Fork. Ergänzt [SCINTILLA-ARCHITECTURE.md](SCINTILLA-ARCHITECTURE.md) Phase 8 und [STREAM-RECOVERY.md](STREAM-RECOVERY.md).

**Ziel:** Keine Pufferüberläufe und keine unsicheren String-APIs auf Pfaden mit Modell-/Netzwerkdaten.

**Nicht Phase 8:** Stream-Recovery-Verhalten (R1–R3), neue Features. Chat-Scintilla war **Phase 12** (erledigt) — String-Audit dort bei Bedarf separat.

---

## Recovery R2 — erledigt (2026-05-25)

| ID | Status | Inhalt |
|----|--------|--------|
| **R2.2** | erledigt | WANT_READ ohne `displayError`, wenn bereits Stream-Daten |
| **R2.1** | erledigt | `ChatTransportOutcome` in `openai.h` / `chatStreamFinalizeTransport()` |
| **R2.3** | erledigt | Stream-Loop: ohne `response.completed` weiterlesen bis `sslWaitForReadReady` scheitert |
| **R2.4** | erledigt | `chatStreamClassifyOutcome()` → `finishChatStream` nach Transport-Outcome |

Details und DoD: [STREAM-RECOVERY.md](STREAM-RECOVERY.md) Phase R2.

---

## Phase-8-Regeln

1. Kein `sprintf` / `printf(userData)` / `strcpy` / `strcat` auf unbekannte Längen.
2. `snprintf(buf, sizeof(buf), …)` oder `AllocVec(strlen+1)` + `CopyMem` + explizites `'\0'`.
3. `strncpy`: nur mit Zielgröße und **immer** terminieren, wenn Quelle ≥ Limit.
4. Große Puffer: eine dokumentierte Obergrenze (`READ_BUFFER_LENGTH`, `CHAT_OUTPUT_TEXT_EDITOR_CONTENTS_LENGTH`, …) und Fehlerpfad.
5. `strncat`: Restplatz `size - strlen(dest) - 1` vor jedem Aufruf (Chat-Pfade).

---

## Audit-Checkliste (MorphOS-Hotpaths)

| Bereich | Datei | Priorität | Status |
|---------|-------|-----------|--------|
| Chat-Anzeige `snprintf`-Größe | `MainWindow.c` `displayConversation` | hoch | **Fix 8.1** |
| Stream-/HTTP-Read append | `openai.c` `strcat`/`strncat` → bounded | hoch | **Fix 8.1** |
| URL host/path | `openai.c` `downloadFile` | hoch | **Fix 8.1** |
| Conversation UTF-8 copy | `gui.c` `addTextToConversation` | mittel | **Fix 8.1** |
| Statusleiste alloc | `gui.c` `updateStatusBar` | mittel | **Fix 8.1** |
| Image-History load | `MainWindow.c` `configDupString` | niedrig | **Fix 8.2** |
| Config JSON strings | `config.c` `configDupString` | mittel | **Fix 8.2** |
| Chat `strncat` / Stream | `MainWindow.c` `strbufAppend` | hoch | **Fix 8.2** |
| Rexx list buffers | `ARexx.c` `strbufAppend` | niedrig | **Fix 8.2** |
| `displayError` alloc | `gui.c` | mittel | **Fix 8.2** |
| API key requester | `APIKeyRequesterWindow.c` | niedrig | **Fix 8.2** |
| Proxy / Custom server requester | `*RequesterWindow.c` | mittel | **Fix 8.3** |
| Chat system / Voice instructions | `*RequesterWindow.c` | niedrig | **Fix 8.3** |
| ElevenLabs settings | `ElevenLabsSettingsRequesterWindow.c` | niedrig | **Fix 8.3** |
| Image create/copy + conversation copy/load | `MainWindow.c` | mittel | **Fix 8.3** |
| Speech accent (ASL) | `menu.c` `configAssignString` | niedrig | **Fix 8.4** |
| ARexx import paths | `menu.c` `snprintf` + AddPart | niedrig | **Fix 8.4** |
| Conversation role | `gui.c` `snprintf` | niedrig | **Fix 8.4** |
| Code-Viewer view buffer | `gui.c` `snprintf` | niedrig | **Fix 8.4** |
| OpenAI error extract | `openai.c` `CopyMem` | niedrig | **Fix 8.4** |
| ASL save paths | `MainWindow.c`, `CodeBlocksViewer.c` | niedrig | **Fix 8.4** |

---

## Implementierung

### 8.1 (2026-05) — Erster Sweep

- `displayConversation`: `snprintf` mit `CHAT_OUTPUT_TEXT_EDITOR_CONTENTS_LENGTH`, nicht `WRITE_BUFFER_LENGTH`.
- `openai.c`: begrenztes Anhängen an `readBuffer`; URL-Parsing mit `sizeof(hostString)` / `sizeof(pathString)`.
- `gui.c`: `addTextToConversation` — `CopyMem` + NUL; `updateStatusBar` — `snprintf` mit Alloc-Größe.

### 8.2 (2026-05) — Zweiter Sweep

- `configDupString()` in `config.c` / `config.h`; alle JSON-String-Felder beim Laden.
- `strbufAppend()` in `gui.c` / `gui.h` — Chat-Ausgabe, Stream-Puffer, Markdown-Hilfen, ARexx-Listen.
- `displayError`: Alloc-Größen, OOM-Guard, `errorTitle` freigeben.
- `APIKeyRequesterWindow.c`: `configDupString` statt `strncpy`.

### 8.3 (2026-05) — Requester + Copy-Pfade

- Alle verbleibenden Config-Requester: `ProxySettings`, `CustomServer`, `ChatSystem`, `VoiceInstructions`, `ElevenLabsSettings` → `configDupString`.
- `MainWindow.c`: neues Bild (`configDupString` + OOM), `copyGeneratedImage`, `copyConversation`, Chat-History-Laden, Auto-Titel.

### 8.3.1 (2026-05) — Requester UAF-Fix

- `configAssignString()`: zuerst duplizieren, dann altes Feld freigeben (MUI `String` nutzt oft direkt `config.*` als `MUIA_String_Contents`).
- Symptom ohne Fix: Proxy leeren, speichern, beenden → System-Freeze (Use-after-free beim Shutdown).

### 8.4 (2026-05) — Restliche feste Puffer

- `menu.c`: `configAssignString` für Speech-Accent; ARexx-Import `snprintf` + `AddPart` (OOM-Guard).
- `gui.c`: `snprintf` für `conversationNode->role` und Code-Viewer-Puffer.
- `openai.c`: `extractUserFriendlyErrorMessage` — `CopyMem` statt `strncpy`.
- `MainWindow.c` / `CodeBlocksViewer.c`: ASL-Save-Pfade mit `snprintf` + `AddPart`.
- **`src/`:** kein `strncpy` mehr (Stand 8.4).

### Danach (optional)

- Host-Tests nur wo schon `src/test/` (kein Pflicht-DoD für MorphOS).

---

## DoD Phase 8 (Minimal)

- Keine bekannten Überläufe auf den Pfaden Chat-Stream, `displayConversation`, `openai` Read/URL in 8.1.
- Dieses Dokument + Roadmap in SCINTILLA-ARCHITECTURE aktualisiert.
- MorphOS-Build + Paket nach Änderungen (Workflow-Regel).

---

*Stand: 2026-05-25 — Phase 8.1–8.4 erledigt; Phase 9: [PHASE-9-DEBUG-LOGS.md](PHASE-9-DEBUG-LOGS.md); v0.1 DoD: [PHASE-10-DOD.md](PHASE-10-DOD.md).*
