# Dokumentation (Fork weiseb78 / Branch scintilla)

**Projektordner:** nur `~/development/morphos/AmigaGPT` in WSL — nicht `C:\Users\xbox\cursorWorkspace\AmigaGPT`. Details: [WSL-SETUP-STATUS.md](WSL-SETUP-STATUS.md).

| Dokument | Inhalt |
| -------- | ------ |
| [HANDLUNGSANWEISUNG-GIT.md](HANDLUNGSANWEISUNG-GIT.md) | Git (kein Commit auf `master`) **und** Begriff **Paketieren** = Z:-Deploy |
| [GIT-FORK-WORKFLOW.md](GIT-FORK-WORKFLOW.md) | Remotes `origin` / `upstream`, Sync, PRs |
| [SCINTILLA-ARCHITECTURE.md](SCINTILLA-ARCHITECTURE.md) | Plan Scintilla.mcc, Streaming, UTF-8 |
| [BUILD-MORPHOS-WSL.md](BUILD-MORPHOS-WSL.md) | MorphOS Cross-Build: WSL2, Debian, BIGFOOT, FlexCat, Make |
| [MORPHOS-SDK-ERGAENZUNGEN.md](MORPHOS-SDK-ERGAENZUNGEN.md) | **Welche SDK-Ergänzungen** genau nötig sind (BIGFOOT vs. AmigaSDK-gcc) |
| [SUDO-NACHINSTALL.md](SUDO-NACHINSTALL.md) | Einmalige `sudo apt` / Rechte / optional `/opt/amiga` |
| [MORPHOS-PROTECTION-BITS.md](MORPHOS-PROTECTION-BITS.md) | Protection Bits, `fix-protection.rexx`, Cross-LHA → Share |
| [WSL-SETUP-STATUS.md](WSL-SETUP-STATUS.md) | **Ist-Stand** der eingerichteten WSL-Umgebung (was erledigt ist, was fehlt) |

## Bauen & Paketieren (WSL)

```bash
cd ~/development/morphos/AmigaGPT
export PATH="$HOME/development/morphos/flexcat/src/bin_unix:$PATH"
make -f Makefile.MorphOS              # je Lauf: APP_VERSION_PATCH + BUILD_NUMBER
./package-morphos-cross.sh            # BUILD=0 überspringt Make, wenn Binary schon da
```

**Paketieren** (verbindlich): siehe [HANDLUNGSANWEISUNG-GIT.md](HANDLUNGSANWEISUNG-GIT.md) Abschnitt 8 — heißt: Skript **`package-morphos-cross.sh`** mit Standard **`DEPLOY=1`** bis zum **erfolgreichen** Kopieren nach **`Z:\morphos\out-crosscompile\`** (LHA + Ordner `package-morphos`). Schlägt der Deploy fehl, ist der Lauf **fehlgeschlagen** (Exit 1), nicht „fast fertig“.

Ergebnis bei Erfolg: zusätzlich `out/AmigaGPT-MorphOS-cross.lha` und Staging unter `out/package-morphos/`. Anleitung im Archiv: `INSTALL-MORPHOS-CROSSBUILD.txt`. SDK/Paket-Details: [MORPHOS-SDK-ERGAENZUNGEN.md](MORPHOS-SDK-ERGAENZUNGEN.md).

**`DEPLOY=0`:** nur LHA/Staging **ohne** Z: — bewusst für Umgebungen ohne Netzlaufwerk; **kein** abgeschlossenes Paketieren im projektinternen Sinn. Wenn Z: zeitweise fehlt: Ursache beheben und Skript **mit** Deploy erneut ausführen (oder von Hand nach `Z:\…` kopieren und den Lauf intern trotzdem als „Deploy manuell nachgeholt“ dokumentieren — technisch prüft das Skript nur den automatischen Schritt).

**Cross-Build vs. echter Test:** `make -f Makefile.MorphOS` prüft nur **Übersetzen und Linken** auf die MorphOS-Toolchain. Ob die App auf **MorphOS** korrekt läuft (UI, Netz, Zeichensatz), sieht man erst auf Hardware oder in einer passenden VM. Host-Tests unter Linux decken nur **portabel ausgelagerte** Logik ab (z. B. `utf8stream`).

## Host-Tests (WSL, ohne MorphOS)

Logik-Tests mit normalem `gcc` — z. B. **UTF-8-Stream-Puffer** (`src/utf8stream.c`):

```bash
~/development/morphos/AmigaGPT/tools/test-utf8stream.sh
```

| Datei | Rolle |
| ----- | ----- |
| `src/test/utf8stream_host_test.c` | Testfälle |
| `src/test/Makefile.utf8stream` | `make test` |
| `tools/test-utf8stream.sh` | Wrapper |
| `out/utf8stream_host_test` | Binary (nach dem Lauf) |

Details zum Modul: [SCINTILLA-ARCHITECTURE.md](SCINTILLA-ARCHITECTURE.md) (Phase 3.2).

## Katalog (Übersetzungen, `AmigaGPT.pot`)

Neue oder geänderte Strings: `catalogs/AmigaGPT.pot` bzw. die `catalogs/*/*.po` anpassen, dann **FlexCat** im `PATH` und Ziel **`make -f Makefile.MorphOS catalog`** (wird auch beim normalen Build mitangezogen, sobald die Katalog-Quellen fehlen). Die Dateien **`src/AmigaGPT_cat.c`** / **`src/AmigaGPT_cat.h`** sind **generiert** — nicht von Hand bearbeiten. Details: [BUILD-MORPHOS-WSL.md](BUILD-MORPHOS-WSL.md) (FlexCat).

## Text / Zeichensatz (MorphOS)

- **Chat (NFloattext):** UTF-8 aus der API → nach Aufbau des Buffers `CodesetsUTF8ToStr` → System-Codeset (wie bisher in `displayConversation()` / Stream-Pfad).
- **Konversationsliste (NList):** `name` UTF-8 für Speichern; Anzeige `name_list_display` (Codesets), siehe Architektur-Dokument Phase 5.1.
- **Assistant mit Code-Fences:** `display_text` ersetzt erkannte Blöcke durch nummerierte Platzhalter `[Codeblock %ld]` **fortlaufend über die ganze Unterhaltung** (nicht pro Antwort neu ab 1). **Ansicht → View code blocks…** listet alle Blöcke des aktuellen Chats (siehe [SCINTILLA-ARCHITECTURE.md](SCINTILLA-ARCHITECTURE.md), Phase 5.2). Während des Live-Streams bleibt der Roh-Text sichtbar; Platzhalter erscheinen nach Ende der Antwort beim Neuzeichnen.
- **Mini-Markdown (Fett/Kursiv):** **Ansicht → Markdown formatting** — abschaltbar (`config.json`: `markdownFormatting`); aus = Rohtext ohne `*`/`_`/`\`-Escape-Probleme im NFloattext.

Agenten: zusätzlich [AGENTS.md](../AGENTS.md) und `.cursor/rules/git-branch-policy.mdc`.
