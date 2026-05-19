# Dokumentation (Fork weiseb78 / Branch scintilla)

**Projektordner:** nur `~/development/morphos/AmigaGPT` in WSL — nicht `C:\Users\xbox\cursorWorkspace\AmigaGPT`. Details: [WSL-SETUP-STATUS.md](WSL-SETUP-STATUS.md).

| Dokument | Inhalt |
| -------- | ------ |
| [HANDLUNGSANWEISUNG-GIT.md](HANDLUNGSANWEISUNG-GIT.md) | Verbindliche Git-Arbeitsweise (kein Commit auf `master`) |
| [GIT-FORK-WORKFLOW.md](GIT-FORK-WORKFLOW.md) | Remotes `origin` / `upstream`, Sync, PRs |
| [SCINTILLA-ARCHITECTURE.md](SCINTILLA-ARCHITECTURE.md) | Plan Scintilla.mcc, Streaming, UTF-8 |
| [BUILD-MORPHOS-WSL.md](BUILD-MORPHOS-WSL.md) | MorphOS Cross-Build: WSL2, Debian, BIGFOOT, FlexCat, Make |
| [MORPHOS-SDK-ERGAENZUNGEN.md](MORPHOS-SDK-ERGAENZUNGEN.md) | **Welche SDK-Ergänzungen** genau nötig sind (BIGFOOT vs. AmigaSDK-gcc) |
| [SUDO-NACHINSTALL.md](SUDO-NACHINSTALL.md) | Einmalige `sudo apt` / Rechte / optional `/opt/amiga` |
| [MORPHOS-PROTECTION-BITS.md](MORPHOS-PROTECTION-BITS.md) | Amiga Protection Bits nach Cross-Build/LHA |
| [WSL-SETUP-STATUS.md](WSL-SETUP-STATUS.md) | **Ist-Stand** der eingerichteten WSL-Umgebung (was erledigt ist, was fehlt) |

## Bauen & Testpaket (WSL)

```bash
cd ~/development/morphos/AmigaGPT
export PATH="$HOME/development/morphos/flexcat/src/bin_unix:$PATH"
make -f Makefile.MorphOS              # je Lauf: APP_VERSION_PATCH + BUILD_NUMBER
./package-morphos-cross.sh            # BUILD=0 überspringt Make, wenn Binary schon da
```

Ausgabe: `out/AmigaGPT-MorphOS-cross.lha` (Staging unter `out/package-morphos/`), optional Deploy nach `Z:\morphos\out-crosscompile\`. Anleitung im Archiv: `INSTALL-MORPHOS-CROSSBUILD.txt`. SDK/Paket-Details: [MORPHOS-SDK-ERGAENZUNGEN.md](MORPHOS-SDK-ERGAENZUNGEN.md).

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

Agenten: zusätzlich [AGENTS.md](../AGENTS.md) und `.cursor/rules/git-branch-policy.mdc`.
