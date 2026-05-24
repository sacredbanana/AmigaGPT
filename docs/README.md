# Dokumentation (Fork weiseb78 / Branch scintilla)

**Projektordner:** nur `~/development/morphos/AmigaGPT` in WSL — nicht `C:\Users\xbox\cursorWorkspace\AmigaGPT`. Details: [WSL-SETUP-STATUS.md](WSL-SETUP-STATUS.md).


| Dokument                                                     | Inhalt                                                                             |
| ------------------------------------------------------------ | ---------------------------------------------------------------------------------- |
| [HANDLUNGSANWEISUNG-GIT.md](HANDLUNGSANWEISUNG-GIT.md)       | Git (kein Commit auf `master`) **und** Begriff **Paketieren** = Z:-Deploy          |
| [GIT-FORK-WORKFLOW.md](GIT-FORK-WORKFLOW.md)                 | Remotes `origin` / `upstream`, Sync, PRs                                           |
| [SCINTILLA-ARCHITECTURE.md](SCINTILLA-ARCHITECTURE.md)       | Plan Scintilla.mcc, Phasen 6–13 (7a/7b erledigt; 7c offen), Streaming              |
| [UNICODE-MORPHOS-MUI.md](UNICODE-MORPHOS-MUI.md)             | Unicode, NFloattext, codesets, MorphOS-Prüfung, Cairo vs. Scintilla                |
| [BUILD-MORPHOS-WSL.md](BUILD-MORPHOS-WSL.md)                 | MorphOS Cross-Build: WSL2, Debian, BIGFOOT, FlexCat, Make                          |
| [MORPHOS-SDK-ERGAENZUNGEN.md](MORPHOS-SDK-ERGAENZUNGEN.md)   | **Welche SDK-Ergänzungen** genau nötig sind (BIGFOOT vs. AmigaSDK-gcc)             |
| [MORPHOS-SDK-NATIV-UND-WSL.md](MORPHOS-SDK-NATIV-UND-WSL.md) | **MorphOS-Voll-SDK** (Referenz) vs. WSL, `devfiles.txt`, Scintilla Phase 6, `pack` |
| [VON-MORPHOS-SDK-SCINTILLA.md](VON-MORPHOS-SDK-SCINTILLA.md) | `**morphos/VonMorphosSDK/Scintilla.guide`** (AmigaGuide), TTEngine, Lexer, API     |
| [SUDO-NACHINSTALL.md](SUDO-NACHINSTALL.md)                   | Einmalige `sudo apt` / Rechte / optional `/opt/amiga`                              |
| [MORPHOS-PROTECTION-BITS.md](MORPHOS-PROTECTION-BITS.md)     | Protection Bits, `fix-protection.rexx`, Cross-LHA → Share                          |
| [WSL-SETUP-STATUS.md](WSL-SETUP-STATUS.md)                   | **Ist-Stand** der eingerichteten WSL-Umgebung (was erledigt ist, was fehlt)        |


## Bauen & Paketieren (WSL)

```bash
cd ~/development/morphos/AmigaGPT
export PATH="$HOME/development/morphos/flexcat/src/bin_unix:$PATH"
make -f Makefile.MorphOS              # je Lauf: APP_VERSION_PATCH + BUILD_NUMBER
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
```


| Datei                             | Rolle                  |
| --------------------------------- | ---------------------- |
| `src/test/utf8stream_host_test.c` | Testfälle              |
| `src/test/Makefile.utf8stream`    | `make test`            |
| `tools/test-utf8stream.sh`        | Wrapper                |
| `out/utf8stream_host_test`        | Binary (nach dem Lauf) |


Details zum Modul: [SCINTILLA-ARCHITECTURE.md](SCINTILLA-ARCHITECTURE.md) (Phase 3.2).

## Katalog (Übersetzungen, `AmigaGPT.pot`)

Neue oder geänderte Strings: `catalogs/AmigaGPT.pot` bzw. die `catalogs/*/*.po` anpassen, dann **FlexCat** im `PATH` und Ziel `**make -f Makefile.MorphOS catalog`** (wird auch beim normalen Build mitangezogen, sobald die Katalog-Quellen fehlen). Die Dateien `**src/AmigaGPT_cat.c**` / `**src/AmigaGPT_cat.h**` sind **generiert** — nicht von Hand bearbeiten. Details: [BUILD-MORPHOS-WSL.md](BUILD-MORPHOS-WSL.md) (FlexCat).

## Text / Zeichensatz (MorphOS)

Ausführlich: [UNICODE-MORPHOS-MUI.md](UNICODE-MORPHOS-MUI.md) (inkl. Prüfung: `GETENV Charset` nicht gesetzt, Emoji im Chat → `??`).

- **Chat-Ausgabe (heute, NFloattext):** UTF-8 intern → `CodesetsUTF8ToStr` → System-Codeset → Anzeige. Geplant **Phase 12:** Scintilla + TTEngine, UTF-8 direkt.
- **Chat-Eingabe:** `TextEditor` → `CodesetsUTF8Create` → UTF-8 zur API — **nicht** Phase 6–10 / 12 (Eingabe).
- **Konversationsliste (NList):** `name` UTF-8; Anzeige `**name_list_display`** (Codesets) — **bereits umgesetzt** (Phase 5.1).
- **Code-Fences / Viewer:** Platzhalter im Chat; **Ansicht → View code blocks…** — **Phase 6** erledigt (Scintilla, UTF-8); **Phase 7a/7b geplant:** NList `index:lang`, ein Block, Copy/Save Raw oder System-Codeset ([SCINTILLA-ARCHITECTURE.md](SCINTILLA-ARCHITECTURE.md)).
- **Mini-Markdown:** **Ansicht → Markdown formatting** — abschaltbar (`config.markdownFormatting`).

**Roadmap:** 6 ✓ · **7a** Block-Liste + Copy UTF-8 · **7b** Save + Copy/Save Codeset · 8–10 · 11 Lexer · **12 Chat** · **13 Worker** — [SCINTILLA-ARCHITECTURE.md](SCINTILLA-ARCHITECTURE.md).

Agenten: zusätzlich [AGENTS.md](../AGENTS.md) und `.cursor/rules/git-branch-policy.mdc`.