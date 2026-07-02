# Handlungsanweisung: MorphOS-Laufzeit (für Agenten)

**Diese Datei ist für AmigaGPT vollständig** (auch bei GitHub-Klon ohne `morphos/docs`).  
Übergreifende MorphOS-Learnings im Workspace: `~/development/morphos/docs/` — siehe [DOKUMENTATION-WORKSPACE.md](DOKUMENTATION-WORKSPACE.md) (keine relativen Links nach oben im Repo).

Verbindliche Ergänzung zu [HANDLUNGSANWEISUNG-GIT.md](HANDLUNGSANWEISUNG-GIT.md) (Build, Paketieren, Commit).  
Gilt für **alle** Änderungen an MorphOS-UI, MUI, Scintilla, Menüs, Shutdown und Chat-Anzeige — **nur AmigaGPT**.

---

## 1. Ziel

Der Nutzer testet auf **echter MorphOS-Hardware**. Der Agent liefert **baubare, paketierte** Builds und vermeidet **Regressionen**, die wir schon einmal hatten — insbesondere **Quit → Neustart** und **ASL-Dialoge aus Menüs**.

---

## 2. Entwicklungszyklus (Kurz)

Siehe [HANDLUNGSANWEISUNG-GIT.md](HANDLUNGSANWEISUNG-GIT.md) §8–9:

1. Implementieren → `make -f Makefile.MorphOS` → **sofort** `./package-morphos-cross.sh` (Z:) — **in derselben Agent-Antwort** wie jedes `make`, außer der Nutzer hat explizit anders verlangt. Kurzweg: `./ship-morphos.sh`
2. Version + MD5 melden
3. **Nutzer testet** (inkl. mehrfach Beenden/Neustart, wenn UI betroffen)
4. **Erst dann** commit (nicht auf `master`)

### Übergabe Agent → Nutzer (kein Halbfertig)

Der Nutzer übernimmt erst am **Ende des definierten Agent-Teils**. Solange **Paketieren** (LHA + Z:, siehe [HANDLUNGSANWEISUNG-GIT.md](HANDLUNGSANWEISUNG-GIT.md) §8) nicht gelaufen ist bzw. der Agent den **Exit-Code** und bei Erfolg **Version + MD5 + Z:-Pfad** nicht gemeldet hat, ist die Aufgabe **nicht** „bis zum MorphOS-Test vorbereitet“ — auch wenn `make` grün war.

- **Falsch:** Antwort wirkt abgeschlossen, aber nur `out/AmigaGPT_MorphOS` existiert / kein `./package-morphos-cross.sh`.
- **Richtig:** In derselben Bearbeitung **Bauen + Paketieren** (sofern der Nutzer nicht ausdrücklich „ohne Paket“ / `DEPLOY=0` gesagt hat), dann klare **Übergabezeile** („Dein Teil: MorphOS …“).

`.cursor/rules/morphos-build-package.mdc` ist die Kurz-Checkliste für Cursor; **`.cursor/rules/morphos-hard-gates.mdc`** die harten Stopps (Z:-Fehler, kein Commit ohne Test). Dieser Abschnitt ist die verbindliche Einordnung für Mensch und Agent.

---

## 2b. Harte Stopps (nicht verhandelbar)

Diese Punkte gelten **immer** — auch wenn der Nutzer „fertig machen“, „weiter“ oder einen Merge abschließen will.

| Stop | Bedingung | Agent darf **nicht** |
| ---- | --------- | -------------------- |
| **Paket** | `package-morphos-cross.sh` / `ship-morphos.sh` mit **`DEPLOY=1`** endet mit Exit ≠ 0 (z. B. `Z:` nicht gemountet) | Als „paketiert“ oder „fertig“ melden; `DEPLOY=0` nachziehen; committen; pushen |
| **Paket** | Nur `out/AmigaGPT-MorphOS-cross.lha` lokal, kein Z:-Deploy | MorphOS-Übergabe behaupten |
| **Test** | Nutzer hat MorphOS-Laufzeit **nicht** als OK bestätigt | `git commit` / `git push` (auch nicht „Merge fertig machen“) |
| **Git** | Branch ist `master` | Committen (Topic-Branch: `feature/*`, `fix/*`, `chore/*`) |

**Nutzer-Meldung bei Z:-Fehler (Pflichtformulierung):**

> **Fehler:** Paketieren fehlgeschlagen — bitte **`Z:` mounten** (Netzlaufwerk / gleiche Windows-Session wie WSL-Deploy). Bis zum erfolgreichen Deploy **blockiert** — kein Commit.

`DEPLOY=0` ist **kein** Ersatz für einen fehlgeschlagenen Z:-Lauf — nur wenn der Nutzer **von vornherein** ohne Deploy arbeiten wollte.

---

## 3. Quit / Neustart (kritisch — Regression vermeiden)

**Problem (mehrfach aufgetreten):** App startet nach mehrfachem Beenden/Neustart nicht oder Konversationsliste „orange“/kaputt.

**Ursache (bekannt):** Falsche Shutdown-Reihenfolge, doppeltes `MUI_Application_Load`, ENVARC-Reload beim Dispose, Pens nach geschlossenem Fenster freigegeben.

### Agent MUSS bei UI-/Shutdown-Änderungen

| Regel | Umsetzung im Code |
| ----- | ----------------- |
| **Ein** `MUIM_Application_Load` vor Fenster-Open | In `createMainWindow()` — **kein** zweites Load nach `loadConversations()` |
| **Letzte Konversation** | `ENVARC:AmigaGPT/last-conversation` — nicht `AMIGAGPT:` für UI-Zustand; nach `loadConversations()` wiederherstellen (siehe [MORPHOS-STABILITAET.md](MORPHOS-STABILITAET.md) §1b) |
| **Config / API-Keys** | `ENVARC:AmigaGPT/config.json` — Lesen mit Fallback `AMIGAGPT:config.json`, Schreiben nur ENVARC; Log: `config read envarc` / `config read fallback amigagpt` |
| **Kein** `ConfigureForScreen`-Hook auf `MUIA_Window_Screen`, der ENVARC lädt und Fenster wieder öffnet | Entfernt in `360a0dd`; nicht zurückbauen |
| **`mainWindowPrepareShutdown()`** vor `MUI_DisposeObject(app)` | In `shutdownGUI()` |
| **Pens freigeben, solange Fenster/Screen noch gültig** | `mainWindowReleasePens()` **vor** `MUIA_Window_Open, FALSE` |
| Konversations-/Bilderliste leeren **vor** Fenster zu | `MUIM_NList_Clear` mit `MUIA_NList_Quiet` (Hauptfenster) |
| Codeblock-Liste beim Shutdown | **Kein** `NList_Clear` — nur `codeBlocksViewerCloseWindow()`; Clear nur bei Chat-Wechsel (`Dismiss`) |
| Chat-Scintilla leeren vor Dispose | **Nicht mehr** — kein `ClearDocument` beim Quit (Freeze-Risiko); siehe [MORPHOS-STABILITAET.md](MORPHOS-STABILITAET.md) §2 |
| Scintilla-Notify-Klasse | `chatOutputScintillaDisposeNotifyClass()` **nach** `MUI_DisposeObject(app)` |
| Laufender Chat-Stream beim Quit | `openAIChatStreamRequestCancel()`; kein `finishChatStream`/`displayConversation` bei Shutdown |
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
| Anzeige | Read-only **Scintilla** + TTEngine (`SC_CP_UTF8`), nicht NFloattext/Codesets — **nur MorphOS**; OS3/OS4/AROS: NFloattext unverändert |
| User/Assistant | `SCLEX_NULL` + Style-Bytes (User = fett/blau/grau); **kein** Ausrichtungs-Menü auf MorphOS (`#ifndef __MORPHOS__` in `menu.c` — nur OS3/OS4 NFloattext) |
| **`SCLEX_MARKDOWN` im Chat** | **Nicht** — Lexer wirkungslos; Menü „Markdown formatting“ schaltet **Midi-Markdown** (`chatOutputScintillaBuildMidiMarkdownDisplay`) |
| **Zeilenumbruch** | Menü *Wrap long lines (chat)*, `config.chatLineWrap` → `chatOutputScintillaApplyLineWrap()` — **nur MorphOS** (`#ifdef __MORPHOS__` in `menu.c`); kein Port auf NFloattext-Targets |
| Stream | Roh-UTF-8 während Stream; kein Markdown-Parse pro Chunk (R3). **MorphOS:** `morphosChatStreamRawScintillaRefresh` — live nur `SetUtf8TextWithRoleStyles` (roh), Midi-Markdown/Links erst nach `finishChatStream` → `displayConversation`. |
| Code-Fences | Parser in `codefence.c` (3+ Backticks, CommonMark-Schließen); Chat `[Codeblock n]` oder Roh-`raw_utf8`; **Export raw**; Host-Test `tools/test-codefence.sh` |
| Chat-Wechsel | `displayConversation` → volle Markdown-Pipeline; Heading-Zeilen **einmal pro Zeile** berechnen (kein Rückscan pro Byte) |
| Scintilla-Wheel zwischen Fenstern | **Bekannte MorphOS/Scintilla.mcc-Einschränkung:** Rest-Scroll kann beim Wechsel Chat-Scintilla ↔ Code-Scintilla übernommen werden (auch in anderen Scintilla-Apps reproduzierbar). App-seitige Workarounds nur zurückhaltend einsetzen, da sie Stabilität/UX verschlechtern können. |

Details: [PHASE-12-CHAT-SCINTILLA.md](PHASE-12-CHAT-SCINTILLA.md), [MIDI-MARKDOWN-ROADMAP.md](MIDI-MARKDOWN-ROADMAP.md).

---

## 6. Prioritäten Roadmap (Nutzer-Vorgaben)

| Prio | Thema | Status |
| ---- | ----- | ------ |
| 1 | **Export Chat (raw UTF-8)** — Diagnose | Menü *Export chat (raw UTF-8)…*, `ChatExport.c` |
| 2 | Midi-Markdown: nur außerhalb Fences, Assistant, `**`/`*`/`__`, `#` | **Erledigt** (MorphOS, Branch `scintilla`) |
| 3 | Klick `[Codeblock]` → Code-Viewer (`SCN_HOTSPOTRELEASECLICK`, Epoch) | **Erledigt** (MorphOS, Branch `scintilla`) |
| 4 | Einfache Pipe-Tabellen (Anzeige, nach Links) | **Erledigt** (`chatOutputScintillaFormatPipeTables`); optional später: eigener Block/NList |
| 5 | Zeilenumbruch Chat (`config.chatLineWrap`, nur MorphOS) | **Erledigt** |
| — | Phase 13 Worker/UI-Batching | Zurückgestellt (R3 reicht vorerst) |

**Nicht** ohne Nutzer-Anlass: Fence-Parser-Tests erzwingen, `SCLEX_MARKDOWN` im Chat erneut „probieren“.

---

## 7. Typische Agent-Fehler

| Fehler | Richtig |
| ------ | ------- |
| „Paketiert“ obwohl Z:-Deploy fehlgeschlagen | **STOP** — Exit-Code + „bitte Z: mounten“; **kein** Commit/Push |
| `DEPLOY=0` nach Z:-Fehler, um weiterzumachen | Verboten; Nutzer behebt `Z:`, Agent wiederholt `ship-morphos.sh` |
| Commit nach Build ohne MorphOS-OK | Warten auf Nutzer-Rückmeldung — auch bei „fertig machen“ / Merge |
| Commit/Push auf `master` | Topic-Branch; Merge erst nach Test |
| ASL direkt im Menü-Hook | `PushMethod` + Deferred-Hook |
| Pens in `shutdownGUI()` nach Fenster zu | Pens in `mainWindowPrepareShutdown()` **vor** Close |
| `codeBlocksViewerDismiss()` beim Shutdown (macht `NList_Clear`) | `codeBlocksViewerCloseWindow()` in `mainWindowPrepareShutdown()` |
| Markdown-Lexer im Chat „schnell testen“ | Midi-Markdown-Plan lesen; Scintilla-Styles portieren |
| Neues Feature + Restart nicht testen lassen | Restart-Checkliste §3 nennen |
| Antwort klingt „fertig“ nach nur `make` | Zuerst `package-morphos-cross.sh`, dann Version+MD5+Z:; sonst kein MorphOS-Übergabe |
| „Fertig machen“ als Freibrief für Commit | **Nein** — erst Z:-Deploy OK, dann Nutzer-Test OK, dann Commit auf Topic-Branch |

Cursor: `.cursor/rules/morphos-build-package.mdc`, **`.cursor/rules/morphos-hard-gates.mdc`**, `.cursor/rules/morphos-runtime-agent.mdc`

---

## 8. Verwandte Dokumente

- [MORPHOS-STABILITAET.md](MORPHOS-STABILITAET.md) — **umgesetzte Stabilitätsmaßnahmen** (Shutdown, Scintilla, Neustart, Log)
- [HANDLUNGSANWEISUNG-GIT.md](HANDLUNGSANWEISUNG-GIT.md) — Git, Paketieren, Commit-Zyklus  
- [SCINTILLA-ARCHITECTURE.md](SCINTILLA-ARCHITECTURE.md) — Phasen, Parser, Stream  
- [STREAM-RECOVERY.md](STREAM-RECOVERY.md) — R1–R3  
- [PHASE-12-CHAT-SCINTILLA.md](PHASE-12-CHAT-SCINTILLA.md) — Chat-Scintilla  
- [CHAT-FIND-SCINTILLA.md](CHAT-FIND-SCINTILLA.md) — Chat-Suche + User-Sprünge (MorphOS)  
- [MIDI-MARKDOWN-ROADMAP.md](MIDI-MARKDOWN-ROADMAP.md) — Markdown/Export-Reihenfolge  
