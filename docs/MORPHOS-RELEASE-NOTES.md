# MorphOS Release Notes (Fork weiseb78)

Nutzerlesbare Übersicht der **Fork-Releases** auf MorphOS.  
Technische Details: [MORPHOS-STABILITAET.md](MORPHOS-STABILITAET.md), Phase-Docs, Git-History.

## Changelog vs. diese Datei

| Datei | Pflege |
| ----- | ------ |
| **`CHANGELOG.md`** (Repo-Root) | **Upstream** (sacredbanana/AmigaGPT). Bei `upstream`-Merge mitpflegen — nicht für jeden MorphOS-Build erweitern. |
| **`MORPHOS-RELEASE-NOTES.md`** (diese Datei) | **Fork**: grobe Release-Linien (z. B. 2.18), was Nutzer auf MorphOS merken. |
| **`DEPLOY-HISTORY.txt`** (Z:) | Automatisch: Build-Nummer, MD5 pro Paketierung. |

Versionsnummer in der App: [MORPHOS-VERSION.md](MORPHOS-VERSION.md) (`MAJOR.MINOR.BUILD_NUMBER`, z. B. **2.18.8785**).

---

## 2.18 — Scintilla-Chat & Stabilität (2026-05)

**Erster Merge dieser Linie nach `master`:** Build **8785** (`1f3264c`).  
Basis upstream: **2.17.0** (Dezember 2025). Nur **MorphOS** in diesem Block vollständig umgesetzt und auf Hardware getestet.

### Chat & Anzeige

- Chat-Ausgabe über **Scintilla + TTEngine** (UTF-8 direkt, User-/Assistant-Farben).
- **Mini-Markdown** im Chat (abschaltbar): Fett/Kursiv, Überschriften, Inline-Code, GFM-Pipe-Tabellen.
- **Hotspots:** `[Codeblock n]` öffnet den Code-Viewer; URLs und Markdown-Links starten den Browser.
- Menü **Ansicht:** Markdown an/aus, Zeilenumbruch, feste Schriftbreite, Chat-Schriftgröße.
- **Stream:** Live-Antworten während des Streams; volles Markdown danach (bei großen Chats Markdown testweise aus lassen).
- **Chat exportieren** als Roh-UTF-8 (Diagnose).

### Code-Blöcke

- Eigenes Fenster **Codeblöcke** mit Scintilla und Syntax-Highlighting.
- Copy/Save UTF-8 und System-Codeset; sicheres Schließen beim Chat-Wechsel und Quit.

### Einstellungen & Daten

- **ENVARC:** `config.json` und zuletzt gewählter Chat (`last-conversation`) persistent über Neustart.
- **Fenster-Geometrie** (Position/Größe) in `config.json` — kein MUI-ENVARC-Save beim Quit (Stabilität).

### Beenden & Neustart

- Quit ohne **System-Freeze** bei großen Chats (Scintilla-Shutdown, kein ENVARC-Save beim Beenden).
- **Relaunch-Locks** auf `T:` koordinieren schnelle Workbench-Neustarts (Stress-Restart getestet ab Build **8785**).
- Nach Quit kurz warten (~3–5 s), dann **einmal** neu starten — nicht doppelklicken.

### Diagnose (optional)

- `"debugLifecycleLog": true` in `ENVARC:AmigaGPT/config.json` → ausführliches Lifecycle-Log auf `AMIGAGPT:`.
- **`T:amigagpt_shutdown.last`** — letzte Shutdown-Phase (eine Zeile, RAM).
- **`T:amigagpt_startup.last`** — letzte Startup-Phase bei blockiertem Start (eine Zeile, RAM).

### Bewusst zurückgestellt

- **Phase 13** (Worker/UI-Batching) — nur bei erneuten Freezes unter Last.

### Bekannte Restrisiken

- Sehr große Chats **mit Markdown an** können langsamer sein als Raw-Modus.
- Scintilla-Mausrad zwischen Chat- und Code-Fenster — plattformbedingt eingeschränkt.

---

## Ältere Fork-Stände

Vor **2.18** auf MorphOS: Code-Viewer-Basis, Stream-Recovery (R1–R3), Export — siehe Git-History ab `360a0dd` und [SCINTILLA-ARCHITECTURE.md](SCINTILLA-ARCHITECTURE.md).

Upstream-Änderungen ab **2.17.0**: [CHANGELOG.md](../CHANGELOG.md).
