# Phase 10 — Definition of Done (Code-Viewer v0.1, Phasen 6–10)

Branch **scintilla**, **MorphOS** (Cross-Build + Test auf Hardware). Phasen **6–10** sind **ein** Release-Block; dieses Dokument ist die **formale Abhakung** — keine neuen Features.

Verwandt: [SCINTILLA-ARCHITECTURE.md](SCINTILLA-ARCHITECTURE.md) (Plan), [PHASE-8-STRING-SAFETY.md](PHASE-8-STRING-SAFETY.md), [PHASE-9-DEBUG-LOGS.md](PHASE-9-DEBUG-LOGS.md), [STREAM-RECOVERY.md](STREAM-RECOVERY.md) (R1–R3 parallel, R2-Rest nicht Blocker).

---

## Scope

| Enthalten | Ausgeschlossen (spätere Phasen) |
|-----------|----------------------------------|
| Fenster **View code blocks…** (Scintilla + NList) | Syntax-Highlighting / Lexer (**11**) |
| Copy/Save UTF-8 + System-Codeset (**7b**) | Chat-Hauptausgabe → Scintilla (**12**) |
| Fence-Parser + Chat-Platzhalter (unverändert) | Worker / UI-Batching (**13**) |
| Mausrad-Scroll Chat (**7c**) | Chat-Eingabe-Editor, Cairo, volles Markdown |
| String-Safety-Sweep (**8**) | Raw-Stream-Log (`T:amigagpt_raw.log`) |
| Optionale Debug-Logs (**9**, minimal) | **R2.1/R2.3/R2.4** vollständig (dokumentiert, nicht Blocker) |

**Recovery:** R1 + R3 **erledigt**; R2.2 **erledigt**; R2.1/R2.3/R2.4 **offen** — blockiert **nicht** Phase-10-DoD (siehe [STREAM-RECOVERY.md](STREAM-RECOVERY.md)).

---

## Gesamt-DoD (v0.1)

| # | Kriterium | Status |
|---|-----------|--------|
| G1 | MorphOS-Cross-Build ohne Link-Fehler (`make -f Makefile.MorphOS`) | ✓ |
| G2 | Paketierung = LHA auf `Z:\morphos\out-crosscompile\` (Exit 0) | ✓ |
| G3 | Alle Pflicht-Checklisten **6–10** unten ✓ | ✓ |
| G4 | Keine bekannten Showstopper für Code-Viewer + normalen Chat (offene R2-Punkte dokumentiert) | ✓ |
| G5 | Doku: dieses Dokument + Roadmap in SCINTILLA-ARCHITECTURE aktualisiert | ✓ |

**Release-Label:** Code-Viewer **v0.1** (Phasen 6–10). Nächste Arbeit: **11** (Lexer/Komfort) oder **12** (Chat-Scintilla) — getrennt planen.

---

## Phase 6 — Scintilla im Code-Viewer

| # | Kriterium | Nachweis | Status |
|---|-----------|----------|--------|
| 6.1 | Menü **View code blocks…** öffnet Fenster mit **NList** + **Scintilla** (MorphOS) | `gui.c`, `CodeBlocksViewer.c` | ✓ |
| 6.2 | Scintilla zeigt nur `raw_code` (UTF-8), `SCI_SETCODEPAGE` UTF-8, read-only | `CodeBlocksScintilla.c` | ✓ |
| 6.3 | `MUIC_Scintilla` in Application; `ttengine.library` für Anzeige | `gui.c`, Build | ✓ |
| 6.4 | OS3/OS4 unverändert (NFloattext, kein Scintilla im Makefile) | `#ifndef __MORPHOS__` | ✓ |
| 6.5 | MorphOS: Kyrillisch/UTF-8 im Viewer, Copy nach Flow Studio OK | Test 2026-05 (Referenzfall) | ✓ |

---

## Phase 7 — Auswahl, Copy, Save, Scroll

### 7a — UI + Copy UTF-8

| # | Kriterium | Nachweis | Status |
|---|-----------|----------|--------|
| 7a.1 | NList: `index:language` (z. B. `3:python`, leer → `-`) | `CodeBlocksViewer.c` | ✓ |
| 7a.2 | Listenauswahl lädt aktiven Block in Scintilla | Viewer-Cache | ✓ |
| 7a.3 | Copy UTF-8 + Strg+C im Code-Fenster; Menü Bearbeiten→Kopieren = aktiver Block | Buttons + `menu.c` | ✓ |
| 7a.4 | Menü deaktiviert wenn keine Blöcke (kein Fehlerdialog) | `refreshViewCodeBlocksMenuState` | ✓ |

### 7b — Save + System-Codeset

| # | Kriterium | Nachweis | Status |
|---|-----------|----------|--------|
| 7b.1 | Vier Buttons: Copy/Save UTF-8 + System | `CodeBlocksViewer.c` | ✓ |
| 7b.2 | Save per ASL, Parent `mainWindow`, deferred `PushMethod` | `MainWindow.c` | ✓ |
| 7b.3 | Schreiben `Open`/`Write`/`Close`; ASL-Abbruch ohne Fehlerdialog | Viewer | ✓ |
| 7b.4 | `codeBlocksViewerPrepareShutdown()` vor `MUI_DisposeObject` — Quit stabil | Test 2026-05 (`1048e16`) | ✓ |

### 7c — Mausrad Chat

| # | Kriterium | Nachweis | Status |
|---|-----------|----------|--------|
| 7c.1 | Mausrad über Chat scrollt NList/NFloattext (Up/Down, Shift = Page) | `MainWindow.c` | ✓ |
| 7c.2 | Custom-Class-Dispose **nach** App-Dispose (kein Privilege Violation) | `chatOutputWheelDisposeClass` | ✓ |

---

## Phase 8 — String-Safety (Minimal-DoD)

Siehe [PHASE-8-STRING-SAFETY.md](PHASE-8-STRING-SAFETY.md#dod-phase-8-minimal).

| # | Kriterium | Status |
|---|-----------|--------|
| 8.1 | Sweep 8.1–8.4: `configDupString`, `strbufAppend`, kein `strncpy` in `src/` | ✓ |
| 8.2 | Requester UAF-Fix (Proxy/API-Keys: `FreeVec` vor Dup, Refresh nach Save) | ✓ (8.3.1, MorphOS 2026-05) |
| 8.3 | Chat-Stream / `displayConversation` / `openai` Read-Pfade auditiert | ✓ |
| 8.4 | R2-Rest bewusst zurückgestellt, dokumentiert | ✓ (nicht Blocker) |

---

## Phase 9 — Debug-Logs (minimal, optional im Alltag)

Siehe [PHASE-9-DEBUG-LOGS.md](PHASE-9-DEBUG-LOGS.md).

| # | Kriterium | Status |
|---|-----------|--------|
| 9.1 | `debugStreamLog` in Config; `T:amigagpt_stream.log` + optional LogTool | ✓ |
| 9.2 | Startzeile `debug logging enabled` nach Neustart | ✓ (MorphOS 2026-05) |
| 9.3 | Erfolgreicher Chat → `stream end outcome=…` | ✓ |
| 9.4 | API-Fehlerpfad → `api_error kind=…` (Web-Search/GPT-3.5-Repro) | ✓ |
| 9.5 | LogTool zeigt Kurzinfo analog GUI-Fehlerdialog | ✓ (Nutzer 2026-05) |
| 9.6 | Keine Secrets in Logs | ✓ (Design) |

---

## Phase 10 — Abnahme (dieses Dokument)

| # | Kriterium | Status |
|---|-----------|--------|
| 10.1 | Checklisten 6–9 vollständig ✓ | ✓ |
| 10.2 | Ausschlüsse 11–13 und R2-Rest in Roadmap sichtbar | ✓ |
| 10.3 | MorphOS-Workflow: Build → Z: → Test → Commit dokumentiert | ✓ [HANDLUNGSANWEISUNG-GIT.md](HANDLUNGSANWEISUNG-GIT.md) §9 |

---

## MorphOS-Regression (Kurz)

Aus [STREAM-RECOVERY.md](STREAM-RECOVERY.md#testplan-morphos) + Phase-7/9-Tests:

1. **Happy path:** Antwort mit Fence → Chat-Platzhalter, Viewer, Copy UTF-8.
2. **Referenzfall:** C + Kyrillisch → Viewer OK, R1-Sync, R3 weniger Freeze.
3. **Quit:** Code-Viewer offen → sauberes Beenden (8.3.1 / Shutdown).
4. **Proxy leeren + Quit:** Requester ohne Guru (8.3.1).
5. **Debug optional:** `debugStreamLog: true` → Datei + LogTool wie GUI.

---

## Offen (bewusst, nach v0.1)

| ID | Thema | Phase |
|----|--------|-------|
| R2.1, R2.3, R2.4 | Transport / PARTIAL-Feintuning | Recovery |
| R4.1, R4.3 | Fence-Heuristik / Host-Tests | Recovery optional |
| 11+ | Lexer, Chat-Scintilla, Worker | SCINTILLA-ARCHITECTURE |

---

*Stand: 2026-05-25 — Phase 10 DoD abgehakt; Phasen 6–10 = Code-Viewer v0.1 abgeschlossen (MorphOS).*
