# Phase 9 — Debug-Logs (Stream / UTF-8)

Branch **scintilla**, MorphOS. Ergänzt [STREAM-RECOVERY.md](STREAM-RECOVERY.md) R4.2 und [SCINTILLA-ARCHITECTURE.md](SCINTILLA-ARCHITECTURE.md) Phase 9.

## Entscheidung

| Mechanismus | Rolle |
|-------------|--------|
| **Datei auf `T:`** | Hauptspur — strukturiert, exportierbar (Z:), ohne LogServer |
| **`KPrintF`** (wenn Log aktiv) | Kurze Zeile in **LogServer / LogTool** (MorphOS ≥ 3.10, System-App) |
| **Remote Logserver** | nicht vorgesehen |

LogTool ersetzt **keine** Datei: die App schreibt nicht „in LogTool“, sondern optional zusätzlich ins System-Debug (LogServer).

## Aktivierung

In `ENVARC:AmigaGPT/config.json` (Fallback beim ersten Start: `AMIGAGPT:config.json`):

```json
"debugStreamLog": true
```

Default: **false**. Nach Änderung App neu starten (oder erneut `readConfig`).

## Dateien

| Pfad | Inhalt |
|------|--------|
| `T:amigagpt_stream.log` | Pro Chat-Stream-Ende eine Zeile |
| `T:amigagpt_utf8.log` | Seltene UTF-8-Puffer-Hinweise |

`T:` ist das übliche MorphOS-RAM-Volume (bei intaktem System immer verfügbar) — nach Reboot leer.

## Stream-Zeile (Format)

```
stream end outcome=OK|PARTIAL|FAILED len=<n> completed=0|1 truncated=0|1 sse=<snippet>
```

- **sse:** letztes `data:`-Fragment aus `openai.c` (max. 200 Zeichen, `\r\n` → `|`)
- Zeilen mit `api_key` / `Bearer` → `sse=(redacted)`

## Code

- `src/streamlog.c` / `streamlog.h`
- Hook: `finishChatStream()` in `MainWindow.c`
- SSE-Snapshot: `openAIChatStreamCaptureLastSse()` in `openai.c`
- UTF-8: `streamLogUtf8()` bei fehlgeschlagenem `utf8stream_append` (Grow)

Link: `-ldebug` (`Makefile.MorphOS`) für `KPrintF`.

## MorphOS-Test

1. Paket mit Phase-9-Build (≥ 2.18.8587) deployen
2. `ENVARC:AmigaGPT/config.json`: `"debugStreamLog": true` (oder `1`) — **App komplett beenden und neu starten** (Log: `config read envarc`)
3. Nach Start: `type T:amigagpt_stream.log` → erste Zeile **`debug logging enabled`**
4. **Erfolgreicher** kurzer Chat (Web-Suche aus oder passendes Modell) → Zeile `stream end outcome=...`
5. Optional **LogTool**: **LogServer** muss laufen; nach `[AmigaGPT]` filtern (nicht jede Ansicht zeigt `KPrintF`)
6. Für Alltag wieder `"debugStreamLog": false`

**Wenn die Datei leer/fehlt:** Config wirklich geladen? (`writeConfig` beim Beenden überschreibt die Datei — Eintrag vor dem Test setzen, dann Neustart). Nur den Web-Search/GPT-3.5-Repro zu testen reicht **nach Fix 9.1** (siehe unten); vorher wurde dieser Pfad nicht geloggt.

## Reproduzierbarer API-Fehler (Test, kein Fix geplant)

**Zweck:** Kontrollierter Fehlerdialog und Log-Zeilen ohne instabiles Netz oder SSL-WANT_READ.

**Ursache:** `webSearchEnabled` hängt in `openai.c` ein Tool `web_search` an; Modelle wie **gpt-3.5-turbo** unterstützen das (bzw. `web_search_preview`) nicht — die API meldet den Fehler, AmigaGPT zeigt die Meldung aus `error.message`.

| Schritt | Einstellung |
|---------|-------------|
| 1 | Chat-Modell **GPT-3.5** (oder anderes Modell **ohne** Web-Search-Unterstützung) |
| 2 | **Web-Suche an** (Menü / `config.json`: `"webSearchEnabled": true`, Default oft schon `true`) |
| 3 | Kurze Chat-Nachricht senden |

**Erwartete API-Meldung (sinngemäß):**

`web_search_preview is not supported with gpt-3.5-turbo`

(Exakter Wortlaut kann von der API leicht abweichen.)

**Erwartetes Verhalten in AmigaGPT:**

- Fehler-Requester / `displayError` mit der API-Meldung
- Kein Guru; App bleibt bedienbar
- Mit `debugStreamLog: true`: Zeile `api_error kind=api_json_error detail=...` in `T:amigagpt_stream.log` (kein `stream end`, weil **kein** `finishChatStream` — das ist normal für diesen Repro)

**Gegenprobe:** Web-Suche **aus** oder Modell mit Web-Search (z. B. neueres GPT) → normaler Chat.

**Hinweis:** Bewusst **nicht** als Bug zu „fixen“ (Modell-Tool-Mismatch); nur als Regressionstest und für Log-/Recovery-Checks.

## Nicht geloggt

API-Keys, Passwörter, volle Prompts, Raw-Socket-Dumps.

---

*Stand: 2026-05-25 — Phase 9 minimal (R4.2); MorphOS getestet; v0.1 DoD: [PHASE-10-DOD.md](PHASE-10-DOD.md).*
