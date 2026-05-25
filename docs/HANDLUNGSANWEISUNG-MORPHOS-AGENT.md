# Handlungsanweisung: MorphOS-Laufzeit (für Agenten)

Verbindliche Ergänzung zu [HANDLUNGSANWEISUNG-GIT.md](HANDLUNGSANWEISUNG-GIT.md) (Build, Paketieren, Commit).  
Gilt für **alle** Änderungen an MorphOS-UI, MUI, Scintilla, Menüs, Shutdown und Chat-Anzeige.

---

## 1. Ziel

Der Nutzer testet auf **echter MorphOS-Hardware**. Der Agent liefert **baubare, paketierte** Builds und vermeidet **Regressionen**, die wir schon einmal hatten — insbesondere **Quit → Neustart** und **ASL-Dialoge aus Menüs**.

---

## 2. Entwicklungszyklus (Kurz)

Siehe [HANDLUNGSANWEISUNG-GIT.md](HANDLUNGSANWEISUNG-GIT.md) §8–9:

1. Implementieren → `make -f Makefile.MorphOS` → `./package-morphos-cross.sh` (Z:)
2. Version + MD5 melden
3. **Nutzer testet** (inkl. mehrfach Beenden/Neustart, wenn UI betroffen)
4. **Erst dann** commit (nicht auf `master`)

---

## 3. Quit / Neustart (kritisch — Regression vermeiden)

**Problem (mehrfach aufgetreten):** App startet nach mehrfachem Beenden/Neustart nicht oder Konversationsliste „orange“/kaputt.

**Ursache (bekannt):** Falsche Shutdown-Reihenfolge, doppeltes `MUI_Application_Load`, ENVARC-Reload beim Dispose, Pens nach geschlossenem Fenster freigegeben.

### Agent MUSS bei UI-/Shutdown-Änderungen

| Regel | Umsetzung im Code |
| ----- | ----------------- |
| **Ein** `MUIM_Application_Load` vor Fenster-Open | In `createMainWindow()` — **kein** zweites Load nach `loadConversations()` |
| **Kein** `ConfigureForScreen`-Hook auf `MUIA_Window_Screen`, der ENVARC lädt und Fenster wieder öffnet | Entfernt in `360a0dd`; nicht zurückbauen |
| **`mainWindowPrepareShutdown()`** vor `MUI_DisposeObject(app)` | In `shutdownGUI()` |
| **Pens freigeben, solange Fenster/Screen noch gültig** | `mainWindowReleasePens()` **vor** `MUIA_Window_Open, FALSE` |
| Konversations-/Bilderliste leeren **vor** Fenster zu | `MUIM_NList_Clear` mit `MUIA_NList_Quiet` (Hauptfenster) |
| Codeblock-Liste beim Shutdown | **Kein** `NList_Clear` — nur `codeBlocksViewerCloseWindow()`; Clear nur bei Chat-Wechsel (`Dismiss`) |
| Chat-Scintilla leeren vor Dispose | `chatOutputScintillaSetUtf8Text(..., "")` |
| `KillNotify` auf `MUIA_Window_Screen` und `CloseRequest` | Vor Dispose |
| Zeiger nullen **nach** Dispose | `mainWindowInvalidateAfterShutdown()` |

### Nutzer-Test (Pflicht bei Shutdown-/Menü-/Fenster-Änderungen)

1. App starten, kurz chatten  
2. **3×** Beenden → neu starten (Workbench-Icon oder Menü Beenden)  
3. Optional: Export, Codeblöcke-Fenster, Konversation wechseln — dann wieder 3× Restart  

**Ohne bestandenen Restart-Test:** Nutzer nicht nach Commit fragen; Fix nachziehen.

Referenz-Commit: `360a0dd` (Basis), Pens-Reihenfolge + Export-Folgen (Branch `scintilla`).

---

## 4. ASL / Datei-Requester aus Menüs

**Problem:** Menüpunkt „Speichern/Export…“ — **kein Dialog**, stiller Abbruch.

**Regel:** Modalen ASL **nicht** direkt aus `MUIM_CallHook` via `MUIV_Notify_Application` aufrufen.

**Muster (wie Codeblock-Speichern, Markdown-Refresh, Fixed-Width-Fonts):**

```c
DoMethod(app, MUIM_Application_PushMethod, app, 2, MUIM_CallHook, &DeferredHook);
```

Im Deferred-Hook: `MUI_AllocAslRequestTags` → `MUI_AslRequestTags` → `MUI_FreeAslRequest` (MorphOS).

Parent-Fenster: `get(mainWindowObject, MUIA_Window, &w)` oder `codeBlocksViewerSetAslParentWindow(mainWindow)` — **Intuition-Window**, nicht MUI-Subwindow.

---

## 5. Chat-Ausgabe (Scintilla, MorphOS)

| Thema | Regel |
| ----- | ----- |
| Anzeige | Read-only **Scintilla** + TTEngine (`SC_CP_UTF8`), nicht NFloattext/Codesets |
| User/Assistant | `SCLEX_NULL` + Style-Bytes (User = fett/blau/grau) |
| **`SCLEX_MARKDOWN` im Chat** | **Nicht** — auf MorphOS-Lexer praktisch wirkungslos; Menü „Markdown formatting“ = No-op bis **Midi-Markdown** |
| Stream | Roh-UTF-8 während Stream; kein Markdown-Parse pro Chunk (R3) |
| Code-Fences | Parser in `codefence.c`; Chat zeigt `[Codeblock n]` oder Roh-`raw_utf8` — Diagnose über **Export raw** |

Details: [PHASE-12-CHAT-SCINTILLA.md](PHASE-12-CHAT-SCINTILLA.md), [MIDI-MARKDOWN-ROADMAP.md](MIDI-MARKDOWN-ROADMAP.md).

---

## 6. Prioritäten Roadmap (Nutzer-Vorgaben)

| Prio | Thema | Status |
| ---- | ----- | ------ |
| 1 | **Export Chat (raw UTF-8)** — Diagnose | Menü *Export chat (raw UTF-8)…*, `ChatExport.c` |
| 2 | Midi-Markdown: nur außerhalb Fences, Assistant, `**`/`*`/`__`, `#` | Geplant |
| 3 | Klick `[Codeblock]` → Code-Viewer | Geplant |
| 4 | Tabellen ggf. als eigener Block (wie Code) | Später |
| — | Phase 13 Worker/UI-Batching | Zurückgestellt (R3 reicht vorerst) |

**Nicht** ohne Nutzer-Anlass: Fence-Parser-Tests erzwingen, `SCLEX_MARKDOWN` im Chat erneut „probieren“.

---

## 7. Typische Agent-Fehler

| Fehler | Richtig |
| ------ | ------- |
| „Paketiert“ obwohl Z:-Deploy fehlgeschlagen | Exit-Code + klare Meldung; kein Commit |
| Commit nach Build ohne MorphOS-OK | Warten auf Nutzer-Rückmeldung |
| ASL direkt im Menü-Hook | `PushMethod` + Deferred-Hook |
| Pens in `shutdownGUI()` nach Fenster zu | Pens in `mainWindowPrepareShutdown()` **vor** Close |
| `codeBlocksViewerDismiss()` beim Shutdown (macht `NList_Clear`) | `codeBlocksViewerCloseWindow()` in `mainWindowPrepareShutdown()` |
| Markdown-Lexer im Chat „schnell testen“ | Midi-Markdown-Plan lesen; Scintilla-Styles portieren |
| Neues Feature + Restart nicht testen lassen | Restart-Checkliste §3 nennen |

---

## 8. Verwandte Dokumente

- [HANDLUNGSANWEISUNG-GIT.md](HANDLUNGSANWEISUNG-GIT.md) — Git, Paketieren, Commit-Zyklus  
- [SCINTILLA-ARCHITECTURE.md](SCINTILLA-ARCHITECTURE.md) — Phasen, Parser, Stream  
- [STREAM-RECOVERY.md](STREAM-RECOVERY.md) — R1–R3  
- [PHASE-12-CHAT-SCINTILLA.md](PHASE-12-CHAT-SCINTILLA.md) — Chat-Scintilla  
- [MIDI-MARKDOWN-ROADMAP.md](MIDI-MARKDOWN-ROADMAP.md) — Markdown/Export-Reihenfolge  

Cursor: `.cursor/rules/morphos-build-package.mdc`, `.cursor/rules/morphos-runtime-agent.mdc`
