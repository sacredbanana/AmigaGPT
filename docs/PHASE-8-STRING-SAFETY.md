# Phase 8 — String- und Sicherheitsdisziplin

Branch **scintilla**, MorphOS-Fork. Ergänzt [SCINTILLA-ARCHITECTURE.md](SCINTILLA-ARCHITECTURE.md) Phase 8 und [STREAM-RECOVERY.md](STREAM-RECOVERY.md).

**Ziel:** Keine Pufferüberläufe und keine unsicheren String-APIs auf Pfaden mit Modell-/Netzwerkdaten.

**Nicht Phase 8:** Stream-Recovery-Verhalten (R1–R3), neue Features, Chat-Scintilla (Phase 12).

---

## Recovery R2 — Rest (bewusst zurückgestellt)

R2 ist **nicht abgeschlossen**. Vor Phase-8-Start festgehalten (2026-05):

| ID | Status | Inhalt |
|----|--------|--------|
| **R2.2** | erledigt | WANT_READ ohne `displayError`, wenn bereits Stream-Daten (`openai.c` + R1-Status) |
| **R2.1** | offen | Outcome PARTIAL/FAILED sauber aus `postChatMessageToOpenAI` / Stream-Loop zurückgeben |
| **R2.3** | offen | `doneReading` bei `bytesRead > 0` ohne `response.completed` — prüfen/dokumentieren |
| **R2.4** | teilweise | `finishChatStream(PARTIAL)` via `chatStreamClassifyOutcome` — Feintuning mit R2.1 |

**Wann nachziehen:** reproduzierbare Transport-Abbrüche oder klare Lücken in Partial-Erkennung — nicht blockierend für Phase 8.

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

### 8.4+ (optional)

- `menu.c` `speechAccent` / Rexx-Pfade; verbleibende `strncpy` in `gui.c` / `openai.c` (feste Puffer).
- Host-Tests nur wo schon `src/test/` (kein Pflicht-DoD für MorphOS).

---

## DoD Phase 8 (Minimal)

- Keine bekannten Überläufe auf den Pfaden Chat-Stream, `displayConversation`, `openai` Read/URL in 8.1.
- Dieses Dokument + Roadmap in SCINTILLA-ARCHITECTURE aktualisiert.
- MorphOS-Build + Paket nach Änderungen (Workflow-Regel).

---

*Stand: 2026-05-24 — Phase 8.1–8.3; R2-Rest dokumentiert; 8.4 optional.*
