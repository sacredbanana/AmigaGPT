# MorphOS: umgesetzte Stabilitätsmaßnahmen

Stand: Branch `scintilla`, Builds ~8720–**8757**.  
Ergänzt [HANDLUNGSANWEISUNG-MORPHOS-AGENT.md](HANDLUNGSANWEISUNG-MORPHOS-AGENT.md) §3 und §5 mit dem **aktuellen Code**.

---

## 1. Ziel

Weniger **System-/App-Freezes** bei Quit, Neustart, Chat-Listenwechsel und großen Scintilla-Dokumenten.  
Diagnose über persistentes Lifecycle-Log (`AMIGAGPT:amigagpt_lifecycle.log`).

---

## 1b. Persistenz: ENVARC, AMIGAGPT, T:

| Ort | Persistent? | Inhalt |
| --- | ----------- | ------ |
| **ENVARC:** | Ja (über Neustart) | MUI: `ENVARC:mui/AmigaGPT.prefs` (`Application_Save`/`Load`); App-Zustand: `ENVARC:AmigaGPT/config.json` (Einstellungen/API-Keys), `ENVARC:AmigaGPT/last-conversation` (zuletzt gewählter Chat) |
| **AMIGAGPT:** | Ja (Daten-Volume) | `chat-history.json`, `image-history.json`, Bilder unter `images/` — **kein** UI-Zustand/Prefs mehr (Legacy: `config.json`, `last-conversation.txt` werden einmalig migriert) |
| **T:** | Nein (RAM) | Relaunch-Locks (`amigagpt_instance.lock`, `amigagpt_teardown.lock`), Spiegel `T:amigagpt_lifecycle.log` |

**Warum nicht nur MUI-ENVARC für die aktive Konversation?**  
`Application_Load` läuft in `createMainWindow()` **vor** `loadConversations()` — die NList ist noch leer; ein zweites Load **nach** `loadConversations()` hat die NList beim Restart kaputt gemacht (nicht wieder einführen). Beim Quit wird die Liste in `mainWindowPrepareShutdown()` **geleert**, **bevor** `Application_Save` — die aktive Zeile landet so oft nicht in `AmigaGPT.prefs`. Daher eigener Eintrag `ENVARC:AmigaGPT/last-conversation` (Chat-**Name**, nach `loadConversations()` per `restoreLastSelectedConversation()`).

Einmalige Migration: falls noch `AMIGAGPT:config.json` oder `AMIGAGPT:last-conversation.txt` existieren, werden sie beim ersten Lesen nach ENVARC übernommen (Lifecycle-Log: `config read fallback amigagpt` → `config migrate amigagpt to envarc`).

---

## 2. Quit / Shutdown

| Maßnahme | Datei / Funktion | Was |
| -------- | ---------------- | --- |
| **Prepare vor Dispose** | `gui.c` → `shutdownGUI()` | `mainWindowPrepareShutdown()` **vor** `MUIM_Application_Save` und `MUI_DisposeObject(app)` |
| **ENVARC nach Prepare** | `shutdownGUI()` | Settings speichern, **nachdem** Fenster/Notifies abgebaut sind (nicht bei aktivem großem styled Chat-Dokument) |
| **Kein Chat-Scintilla-Clear beim Quit** | `mainWindowPrepareShutdown()` | **Kein** `SCI_CLEARALL` / `SETTEXT ""` am Chat — kann OS einfrieren; MUI dispose räumt auf |
| **Kein Code-Scintilla-Clear beim Quit** | `codeBlocksViewerPrepareShutdown()` | Nur `KillNotify`, Zeiger nullen; Log: `scintilla skip clear` |
| **Kein `NList_Clear` Code-Viewer Shutdown** | `codeBlocksViewerPrepareShutdown()` | Clear nur bei Chat-Wechsel (`codeBlocksViewerDismiss`), nicht beim App-Ende |
| **Stream abbrechen** | `mainWindowPrepareShutdown()` | `openAIChatStreamRequestCancel()` — kein `finishChatStream` / `displayConversation` während Shutdown |
| **Deferred Styles abbrechen** | `chatOutputScintillaCancelDeferredStyles()` | Pending `PushMethod`-Styling wird verworfen |
| **Refresh-Queue leeren** | `mainWindowPrepareShutdown()` | `chatOutputRefreshPending` / `FromList` zurücksetzen |
| **Notify abklemmen** | `chatOutputScintillaDetachNotify()` | Vor Dispose, solange Hauptfenster noch offen |
| **Code-Viewer schließen** | `codeBlocksViewerCloseWindow()` in Prepare | **Nicht** erneut in `PrepareShutdown` (CloseRequest-Reentry) |
| **Pens vor Fenster zu** | `mainWindowReleasePens()` vor `MUIA_Window_Open, FALSE` | Screen noch gültig |
| **NList leeren (Hauptfenster)** | `mainWindowEmptyNList()` mit `MUIA_NList_Quiet` | Konversations-/Bilderliste vor Fenster-Close |
| **KillNotify Screen/Close** | `mainWindowPrepareShutdown()` | Kein ENVARC-Reload-Hook auf `MUIA_Window_Screen` |
| **Notify-Klasse nach Dispose** | `shutdownGUI()` | `chatOutputScintillaDisposeNotifyClass()` **nach** `MUI_DisposeObject` |
| **Post-Dispose-Cooldown** | `shutdownGUI()` | `Delay(150)` (~3 s) — MUI/Intuition soll fertig teardownen |
| **Teardown-Marker + Lock frei** | `morphos_relaunch.c` | nur `T:amigagpt_teardown.lock`; create/delete im Lifecycle-Log (`relaunch lock …`) |
| **Single-Instance-Lock** | `morphos_relaunch.c` | nur `T:amigagpt_instance.lock`; kein Fallback ins Programmverzeichnis; nach `mainWindowPrepareShutdown` löschen |
| **Chat-Shutdown** | `MainWindow.c` / `ChatOutputScintilla.c` | Konversationswahl erst nach `morphos conversation select enabled`; vor Dispose Scintilla quiescen + PushMethods flushen; ENVARC-Skip bei >32 KiB Chat-Puffer |
| **Letzte Konversation** | `saveLastSelectedConversationName()` | Vor `currentConversation = NULL` in `mainWindowPrepareShutdown()` sowie bei Listenwahl → `ENVARC:AmigaGPT/last-conversation` |

---

## 3. Startup / Neustart

| Maßnahme | Datei | Was |
| -------- | ----- | --- |
| **Relaunch-Guard** | `main.c` + `morphos_relaunch.c` | Warten auf Teardown-Marker, Instance-Lock, `Delay(75)`, Log `startup instance lock ok` |
| **App-Create-Cooldown** | `gui.c` → `initVideo()` | `Delay(30)` vor erstem `ApplicationObject` |
| **App-Create-Retry** | `initVideo()` | Bis **12** Versuche; Delay 25, ab Versuch 5: **40** Ticks |
| **Einmal Application_Load** | `createMainWindow()` | **Kein** zweites Load nach `loadConversations()` (NList-Restart-Bug) |
| **Code-Fenster nach Load zu** | `createMainWindow()` | ENVARC darf Code-Viewer nicht vor Scintilla-Init öffnen |
| **Chat Scintilla prime/finish getrennt** | `createMainWindow()` + `morphosRunStartupDeferred()` | `chatOutputScintillaPrimeViewer` vor `OM_ADDMEMBER`; volles Init (`FinishViewerInit`) erst in erstem `NewInput` |
| **Code-Scintilla prime at startup** | `gui.c` | `codeBlocksViewerPrimeScintillaAtStartup()` — schweres Init nicht im ersten Fenster-Open |
| **Letzte Konversation wiederherstellen** | `restoreLastSelectedConversation()` nach `loadConversations()` | Liest `ENVARC:AmigaGPT/last-conversation`, setzt NList-Active per Name; Log: `restore last conversation ok` / `miss` |
| **Konversationswahl freigeben** | `morphosEnableConversationSelect()` nach `morphosRunStartupDeferred()` | Erst wenn Scintilla fertig; Log: `morphos conversation select enabled` |
| **Auto-Select Fallback** | `morphosEnableConversationSelect()` | Wenn weder ENVARC-Name noch aktive Zeile: erste Liste (`morphos conversation auto-select first`); sonst `auto-select active`; Laden per `PushMethod` (`conversation select deferred begin`) |
| **Fenster nach vorn** | `createMainWindow()` | `MUIM_Window_ToFront` nach `MUIA_Window_Open, TRUE` |

---

## 4. Chat-Anzeige (Scintilla)

| Maßnahme | Datei | Was |
| -------- | ----- | --- |
| **Deferred UI-Refresh** | `MainWindow.c` | `displayConversation` / Listenklick → `PushMethod` (`morphosScheduleChatOutputRefreshFromList`) — kein synchrones Scintilla-Update im NList-Hook |
| **Deferred Style-Apply** | `ChatOutputScintilla.c` | Nach `SCI_SETTEXT`: Styling immer per zweitem `PushMethod` (`apply styles defer scheduled`) |
| **Run-Length-Styling** | `chatOutputScintillaApplyRoleStyleBytes()` | `SCI_SETSTYLING` (Runs) statt `SCI_SETSTYLINGEX`; kein redundant `SCI_SETLEXER` vor Styling |
| **Kein Scroll bei Listenwechsel** | `chatOutputScintillaMorphosSkipViewport` | Kein `GOTOPOS`/`SCROLLCARET` bei NList-Chat-Wechsel und während Stream |
| **Schweres Scroll vermeiden** | `SetUtf8TextWithRoleStyles()` | Viewport nur wenn nicht Skip und (preserveViewport oder Text ≤ 24 KB) |
| **Raw während Stream** | `morphosChatStreamRawScintillaRefresh` | Live-Antwort ohne Markdown-Parse pro Chunk |
| **Raw/Markdown per Menü** | `config.markdownFormatting` | Haken **aus** → dauerhaft Raw-Pfad; Haken **an** → Markdown außerhalb Stream (Nutzer steuert Stabilitätstest) |
| **Refresh-Race** | `morphosScheduleChatOutputRefresh()` | Setzt `FromList` nicht zurück, wenn bereits ein Listen-Refresh pending ist |
| **Conversation-Klick deferred** | `ConversationRowClicked` → `PushMethod` | Kein `displayConversation` direkt im NList-Notify |

---

## 5. Code-Blocks-Viewer

| Maßnahme | Datei | Was |
| -------- | ----- | --- |
| **Dismiss ohne Scintilla-Clear** | `codeBlocksViewerDismiss()` | Fenster zu + Liste leer; **kein** `CLEARALL` beim Chat-Wechsel |
| **ASL/ Menü deferred** | diverse Hooks | Modal-Dialoge nur über `PushMethod`, nicht aus `MUIV_Notify_Application` |
| **CloseRequest-Hook** | `gui.c` | Nur auf `CloseRequest`, **nicht** auf `MUIA_Window_Open FALSE` (Dispose-Reentry) |

---

## 6. Diagnose (Lifecycle-Log)

| Maßnahme | Datei | Was |
| -------- | ----- | --- |
| **Persistentes Log** | `streamlog.c` | `AMIGAGPT:amigagpt_lifecycle.log` + Spiegel `T:amigagpt_lifecycle.log` |
| **Fein granulare Phasen** | überall `streamLogLifecycle()` | Startup, createMainWindow, chat settext/styling, shutdown, app-create retry |
| **KPrintF-Spiegel** | `streamLogLifecycle()` | `[AmigaGPT lifecycle]` auch auf Debug-Kanal |

Typische **gute** Raw-Kette im Log:

```
chatOutput refresh raw path
chat scintilla settext done
chat scintilla apply styles defer scheduled
chat scintilla setstyling runs done
shutdown … process exit
```

Typisches **Restart-Problem** (Relaunch-Race, noch offen):

```
startup begin          ← neue Instanz
app create fail        ← alte Instanz noch nicht fertig mit MUI
… process exit         ← alte Instanz endet später
```

Typisches **„kein Chat nach Restart“** (8753–8755, mit 8756+ behoben):

```
morphos conversation select enabled
shutdown begin           ← ohne conversation select deferred begin dazwischen
```

Ursache: keine aktive NList-Zeile und kein gespeicherter Chat-Name → mit `ENVARC:AmigaGPT/last-conversation` + Auto-Select-Fallback beheben.

**Nutzer:** Nach Quit auf `process exit` warten (**3–5 s**), dann neu starten; bei Hänger **nicht** doppelklicken.

---

## 7. Priorität / Restrisiken

**Höhere Priorität (normaler Betrieb):** Chat-Scintilla (`settext`/deferred styling), Markdown-Pfad, Stream — betrifft tägliche Nutzung.

**Timebox Restart-Race (dieser Stand):** `morphos_relaunch.c` — Teardown-Wartezeit, ~3 s Post-Dispose, Instance-Lock. Ziel: Stress-Restart und WB-Doppelstart; in normaler Nutzung selten.

**Noch offen:**

- **Zu schneller Neustart** trotz Guard → ggf. längere Delays oder strengeres Warten auf Teardown-Datei.
- **Markdown mit Haken an** nach Stream / bei sehr großen Chats → teurer Pfad (`markdown begin`); bei Freeze Markdown per Menü **aus** testen.
- **Scintilla-Wheel** zwischen Chat- und Code-Fenster — plattformbedingt, nur zurückhaltend workarounden.
- **Totaler System-Freeze** — Log schreibt ggf. nicht bis zum Absturz; nach Reboot `AMIGAGPT:`-Log lesen.

---

## 8. Empfohlener Stabilitätstest (Nutzer)

**Block R (Restart mit Chat):**

1. Menü **Markdown-Formatierung aus** (Raw, in Config gemerkt).
2. Log rotieren: `AMIGAGPT:amigagpt_lifecycle.log` löschen oder umbenennen.
3. Start → **bestehenden** Chat wählen (oder nach 8757: soll ohne Klick laden) → Quit → **3–5 s** Pause.
4. **15–20×** wiederholen; im Log pro Zyklus erwarten: `restore last conversation ok` oder `auto-select …`, dann `conversation select deferred begin` → `… deferred done`, dann `process exit`.
5. About-Version prüfen (z. B. **2.18.8757**).

**Block C (optional):** Eine Session, **10–20×** nur Chat in der Liste wechseln, **ohne** Restart.

**Weiteres:**

- Optional: großen Chat, Code-Viewer öffnen/schließen, dann wieder Block R.
- Wenn stabil: Markdown **an**, gleiche Schleife — vergleichen, ob nur Markdown regressiert.

---

## 9. Verwandte Commits / Bereiche

- Shutdown-Basis: `360a0dd`, Branch `scintilla`
- Scintilla-Lifecycle-Fixes: `MainWindow.c`, `ChatOutputScintilla.c`, `CodeBlocksViewer.c`, `gui.c`, `main.c`, `streamlog.c`
