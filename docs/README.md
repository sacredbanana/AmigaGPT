# Dokumentation (Fork weiseb78 / Branch scintilla)

**Projektordner:** nur `~/development/morphos/AmigaGPT` in WSL — nicht `C:\Users\xbox\cursorWorkspace\AmigaGPT`. Details: [WSL-SETUP-STATUS.md](WSL-SETUP-STATUS.md).


| Dokument                                                     | Inhalt                                                                             |
| ------------------------------------------------------------ | ---------------------------------------------------------------------------------- |
| [HANDLUNGSANWEISUNG-GIT.md](HANDLUNGSANWEISUNG-GIT.md)       | Git (kein Commit auf `master`) **und** Begriff **Paketieren** = Z:-Deploy          |
| [HANDLUNGSANWEISUNG-MORPHOS-AGENT.md](HANDLUNGSANWEISUNG-MORPHOS-AGENT.md) | **Agent:** Restart, ASL/PushMethod, Scintilla-Chat, bekannte Regressionen |
| [MORPHOS-STABILITAET.md](MORPHOS-STABILITAET.md)             | **Umgesetzte Stabilitätsmaßnahmen** (Shutdown, Neustart, Scintilla, Lifecycle-Log) |
| [MORPHOS-VERSION.md](MORPHOS-VERSION.md)                     | `$VER:` Version.Revision (MorphOS Style Guide), `version.h`                        |
| [GIT-FORK-WORKFLOW.md](GIT-FORK-WORKFLOW.md)                 | Remotes `origin` / `upstream`, Sync, PRs                                           |
| [SCINTILLA-ARCHITECTURE.md](SCINTILLA-ARCHITECTURE.md)       | Plan Scintilla.mcc, Phasen 6–13; **6–12 erledigt** (MorphOS)                         |
| [PHASE-10-DOD.md](PHASE-10-DOD.md)                           | **DoD Code-Viewer v0.1** (Phasen 6–10, MorphOS-Abnahme)                            |
| [PHASE-12-CHAT-SCINTILLA.md](PHASE-12-CHAT-SCINTILLA.md)     | **Phase 12 + 12.1** Chat-Scintilla, Midi-Markdown, Hotspots, Testplan (MorphOS)     |
| [MIDI-MARKDOWN-ROADMAP.md](MIDI-MARKDOWN-ROADMAP.md)         | Prioritäten Export / Midi-Markdown / Hotspots / Tabellen / Zeilenumbruch             |
| [PHASE-11-LEXER.md](PHASE-11-LEXER.md)                       | Phase 11 Syntax-Highlighting im Code-Viewer                                          |
| [PHASE-8-STRING-SAFETY.md](PHASE-8-STRING-SAFETY.md)         | Phase 8 String-Safety (8.1–8.4)                                                    |
| [PHASE-9-DEBUG-LOGS.md](PHASE-9-DEBUG-LOGS.md)               | Phase 9 Debug-Logs (Stream / UTF-8)                                                |
| [STREAM-RECOVERY.md](STREAM-RECOVERY.md)                     | Plan Stream- & Chat-Recovery (R1–R4): WANT_READ, UI-Sync, Freeze                   |
| [UNICODE-MORPHOS-MUI.md](UNICODE-MORPHOS-MUI.md)             | Unicode, NFloattext, codesets, MorphOS-Prüfung, Cairo vs. Scintilla                |
| [BUILD-MORPHOS-WSL.md](BUILD-MORPHOS-WSL.md)                 | MorphOS Cross-Build: WSL2, Debian, BIGFOOT, FlexCat, Make                          |
| [MORPHOS-SDK-ERGAENZUNGEN.md](MORPHOS-SDK-ERGAENZUNGEN.md)   | **Welche SDK-Ergänzungen** genau nötig sind (BIGFOOT vs. AmigaSDK-gcc)             |
| [MORPHOS-SDK-NATIV-UND-WSL.md](MORPHOS-SDK-NATIV-UND-WSL.md) | **MorphOS-Voll-SDK** (Referenz) vs. WSL, `devfiles.txt`, Scintilla Code-Viewer (6–7b), `pack` |
| [VON-MORPHOS-SDK-SCINTILLA.md](VON-MORPHOS-SDK-SCINTILLA.md) | `**morphos/VonMorphosSDK/Scintilla.guide`** (AmigaGuide), TTEngine, Lexer, API     |
| [SUDO-NACHINSTALL.md](SUDO-NACHINSTALL.md)                   | Einmalige `sudo apt` / Rechte / optional `/opt/amiga`                              |
| [MORPHOS-PROTECTION-BITS.md](MORPHOS-PROTECTION-BITS.md)     | Protection Bits, `fix-protection.rexx`, Cross-LHA → Share                          |
| [WSL-SETUP-STATUS.md](WSL-SETUP-STATUS.md)                   | **Ist-Stand** der eingerichteten WSL-Umgebung (was erledigt ist, was fehlt)        |


## Bauen & Paketieren (WSL)

```bash
cd ~/development/morphos/AmigaGPT
export PATH="$HOME/development/morphos/flexcat/src/bin_unix:$PATH"
make -f Makefile.MorphOS              # je Lauf: BUILD_NUMBER + Datum; Anzeige = 2.18.<build>
./ship-morphos.sh                       # = make -f Makefile.MorphOS ship (Standard nach Code-Änderung)
make -f Makefile.MorphOS ship           # build + daemon + package + Z:
./package-morphos-cross.sh            # BUILD=0 überspringt Make, wenn Binary schon da
```

**Paketieren** (verbindlich): siehe [HANDLUNGSANWEISUNG-GIT.md](HANDLUNGSANWEISUNG-GIT.md) Abschnitt 8 — WSL legt `**AmigaGPT-MorphOS-cross.lha`** nach **`Z:\morphos\out-crosscompile\`** (nur die LHA, kein Ordner `package-morphos` auf Z:). Ob die LHA auf MorphOS aktuell ist, prüft das **MorphOS-Deploy-Skript** (liegt nicht in diesem Repo). Deploy-Fehler → Exit 1.

Ergebnis in WSL: `out/AmigaGPT-MorphOS-cross.lha` (+ lokales Staging `out/package-morphos/` zum Packen). Anleitung im Archiv: `INSTALL-MORPHOS-CROSSBUILD.txt`.

`**DEPLOY=0`:** nur LHA/Staging **ohne** Z: — bewusst für Umgebungen ohne Netflzlaufwerk; **kein** abgeschlossenes Paketieren im projektinternen Sinn. Wenn Z: zeitweise fehlt: Ursache beheben und Skript **mit** Deploy erneut ausführen (oder von Hand nach `Z:\…` kopieren und den Lauf intern trotzdem als „Deploy manuell nachgeholt“ dokumentieren — technisch prüft das Skript nur den automatischen Schritt).

**Cross-Build vs. echter Test:** `make -f Makefile.MorphOS` prüft nur **Übersetzen und Linken** auf die MorphOS-Toolchain. Ob die App auf **MorphOS** korrekt läuft (UI, Netz, Zeichensatz), sieht man erst auf Hardware oder in einer passenden VM. Host-Tests unter Linux decken nur **portabel ausgelagerte** Logik ab (z. B. `utf8stream`).

## Host-Tests (WSL, ohne MorphOS)

Logik-Tests mit normalem `gcc` — z. B. **UTF-8-Stream-Puffer** (`src/utf8stream.c`):

```bash
~/development/morphos/AmigaGPT/tools/test-utf8stream.sh
~/development/morphos/AmigaGPT/tools/test-chatmd-markers.sh
```


| Datei                             | Rolle                  |
| --------------------------------- | ---------------------- |
| `src/test/utf8stream_host_test.c` | Testfälle              |
| `src/test/Makefile.utf8stream`    | `make test`            |
| `tools/test-utf8stream.sh`        | Wrapper                |
| `out/utf8stream_host_test`        | Binary (nach dem Lauf) |
| `tools/test-chatmd-markers.sh`    | Nur Kursiv-`*` (Delimiter); Fett `**` ohne Extra-Tests — siehe [MIDI-MARKDOWN-ROADMAP.md](MIDI-MARKDOWN-ROADMAP.md#delimiter-policy-keine-markdown-bibliothek) |
| `out/chatmd_markers_host_test`    | Binary (nach dem Lauf) |


Details zum Modul: [SCINTILLA-ARCHITECTURE.md](SCINTILLA-ARCHITECTURE.md) (Phase 3.2).

## Katalog (Übersetzungen, `AmigaGPT.pot`)

Neue oder geänderte Strings: `catalogs/AmigaGPT.pot` bzw. die `catalogs/*/*.po` anpassen, dann **FlexCat** im `PATH` und Ziel `**make -f Makefile.MorphOS catalog`** (wird auch beim normalen Build mitangezogen, sobald die Katalog-Quellen fehlen). Die Dateien `**src/AmigaGPT_cat.c**` / `**src/AmigaGPT_cat.h**` sind **generiert** — nicht von Hand bearbeiten. Details: [BUILD-MORPHOS-WSL.md](BUILD-MORPHOS-WSL.md) (FlexCat).

**Pflege-Policy (Fork):** Aktiv gepflegt wird nur **`catalogs/german/deutsch.po`** (Deutsch). Nach `msgmerge` haben die übrigen `catalogs/*/*.po` für neue Strings typischerweise **`msgstr` = englisches `msgid`** — das ist beabsichtigt (englische UI-Fallbacks), kein Review nötig. Keine maschinellen Übersetzungen in andere Sprachen ohne Muttersprachler-Review.

## Text / Zeichensatz (MorphOS)

Ausführlich: [UNICODE-MORPHOS-MUI.md](UNICODE-MORPHOS-MUI.md) (inkl. Prüfung: `GETENV Charset` nicht gesetzt, Emoji im Chat → `??`).

- **Chat-Ausgabe (MorphOS, Phase 12):** Scintilla + TTEngine, UTF-8 direkt — [PHASE-12-CHAT-SCINTILLA.md](PHASE-12-CHAT-SCINTILLA.md). OS3/OS4/AROS weiter **NFloattext** + codesets (kein Scintilla im Chat).
- **Chat-Ansicht nur MorphOS:** Menü **Ansicht** — *Markdown formatting*, *Wrap long lines (chat)*, Export raw, Codeblöcke-Hotspots, Pipe-Tabellen-Anzeige gelten nur unter `#ifdef __MORPHOS__` (gleicher Grund: nur dort Chat-Scintilla). `config.chatLineWrap` / `config.markdownFormatting` in `config.json` werden auf MorphOS gelesen; auf anderen Targets ohne Chat-Scintilla ohne Wirkung.
- **Chat-Eingabe:** `TextEditor` → `CodesetsUTF8Create` → UTF-8 zur API — **nicht** Phase 6–10 / 12 (Eingabe).
- **Konversationsliste (NList):** `name` UTF-8; Anzeige `**name_list_display`** (Codesets) — **bereits umgesetzt** (Phase 5.1).
- **Code-Fences / Viewer:** Platzhalter im Chat; **Ansicht → Codeblöcke…** — **Phase 6–7b erledigt** (NList, Scintilla, Copy/Save UTF-8 + System). Recovery Chat/Stream: [STREAM-RECOVERY.md](STREAM-RECOVERY.md).
- **Mini-Markdown:** **Ansicht → Markdown formatting** — abschaltbar (`config.markdownFormatting`).
- **Zeilenumbruch (Chat):** **Ansicht → Wrap long lines (chat)** — `SCI_SETWRAPMODE` / `config.chatLineWrap` (Standard: an); nur MorphOS.

**Roadmap:** 6–12 ✓ (MorphOS) · **Export Chat (raw)** ✓ · **Midi-Markdown + 3a/3b Hotspots** (Codeblock + URL) ✓ · **Zeilenumbruch Chat** ✓ — [MIDI-MARKDOWN-ROADMAP.md](MIDI-MARKDOWN-ROADMAP.md) · **13 Worker** zurückgestellt — [PHASE-12-CHAT-SCINTILLA.md](PHASE-12-CHAT-SCINTILLA.md), [STREAM-RECOVERY.md](STREAM-RECOVERY.md).

## Phase 12 — erledigt (MorphOS)

**Status (2026-05):** Hauptfenster-Chat-Ausgabe auf **Scintilla + TTEngine** (UTF-8 direkt); **Phase 12.1 (Midi-Markdown)** abgeschlossen und auf Hardware getestet. Details und Testplan: [PHASE-12-CHAT-SCINTILLA.md](PHASE-12-CHAT-SCINTILLA.md).

| Bereich | Umsetzung |
| ------- | --------- |
| Chat-Scintilla | NFloattext ersetzt; Role-Styles User/Assistant; Restart-Stabilität — [MORPHOS-STABILITAET.md](MORPHOS-STABILITAET.md) |
| Midi-Markdown | `**` / `*` / `__`, `#`-Überschriften, `` `inline code` ``; Emoji→Text nur bei *Markdown formatting* |
| Stream | Live-Roh-UTF-8 (R3); Append-Delta; volles Markdown nach Stream-Ende |
| Hotspots **3a** | `[Codeblock n]` → Code-Viewer |
| Hotspots **3b** | Bare `https://` + `[Label](url)` → `openurl.library` |
| **3c** Pipe-Tabellen | Spalten-Padding nach Links (Assistant, nicht während Stream) |
| Menüs | Markdown, Wrap, Export raw, Feste Schriftbreite, Chat-Schriftgröße |
| **Bewusst entfallen** | User/Assistant-Ausrichtung im Chat (Role-Styles wie Cursor; Menü nur OS3/OS4) |
| **Zurückgestellt** | Phase 13 Worker/UI-Batching — nur bei erneutem Stream-Freeze |

Agenten: zusätzlich [AGENTS.md](../AGENTS.md) und `.cursor/rules/git-branch-policy.mdc`.