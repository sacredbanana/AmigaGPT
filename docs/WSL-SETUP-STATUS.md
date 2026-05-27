# WSL-Setup: Ist-Stand (MorphOS-Entwicklung)

Chronik und Checkliste dessen, was für **AmigaGPT / MorphOS** in **WSL2 (Debian)** eingerichtet wurde. Anleitung zum Nachbauen: [BUILD-MORPHOS-WSL.md](BUILD-MORPHOS-WSL.md).

**Stand:** 2026-05-19 · WSL-User: `weimer`

---

## Ziele

1. MorphOS-Cross-Build von AmigaGPT in WSL (ohne zweiten Compiler).
2. Ein kanonischer Projektordner unter **WSL-Home** (nicht dauerhaft `/mnt/c/…`).
3. Git nur auf Topic-Branch **`scintilla`**, nicht auf `master` — siehe [HANDLUNGSANWEISUNG-GIT.md](HANDLUNGSANWEISUNG-GIT.md).

---

## Umgebung

| Komponente | Status |
| ---------- | ------ |
| WSL2 + Debian | installiert (nach Windows-Neustart) |
| Debian-Pakete | `build-essential`, `git`, `wget`, `make`, `gettext`, … — **ohne** `flexcat` (existiert nicht in APT) |
| WSL-Version prüfen | `wsl -l -v` → Debian **VERSION 2** |

---

## Verzeichnislayout (kanonisch — nur WSL)

**Einziger Projektordner für Git, Build und Doku:**

```text
~/development/
  morphos/
    flexcat/              ← gebaut (FlexCat 2.18)
    AmigaGPT/             ← Git-Clone, Branch scintilla, `make -f Makefile.MorphOS`
```

**Cursor / IDE** (Workspace hier öffnen, nicht unter Windows):

```text
\\wsl$\Debian\home\weimer\development\morphos\AmigaGPT
```

**Nicht mehr verwenden** (veraltet, führt zu CRLF-/Pull-Konflikten):

```text
C:\Users\xbox\cursorWorkspace\AmigaGPT
```

Sync nur noch per Git: `git pull` / `git push` in `~/development/morphos/AmigaGPT` — kein `cp` von `/mnt/c/…` mehr.

---

## Git (Fork)

| Remote | URL |
| ------ | --- |
| `origin` | `https://github.com/weiseb78/AmigaGPT.git` |
| `upstream` | `https://github.com/sacredbanana/AmigaGPT.git` |

- Entwicklung auf **`scintilla`**
- **`master`** lokal = `origin/master` (keine direkten Feature-Commits)
- Regeln: [HANDLUNGSANWEISUNG-GIT.md](HANDLUNGSANWEISUNG-GIT.md), `.cursor/rules/git-branch-policy.mdc`

---

## FlexCat (erledigt)

| Schritt | Details |
| ------- | ------- |
| Quelle | `git clone https://github.com/adtools/flexcat.git` → `~/development/morphos/flexcat` |
| Erster Build | **`make bootstrap`** dann **`make`** (ohne bootstrap: `flexcat: No such file or directory` bei `FlexCat_cat.h`) |
| Binary | `~/development/morphos/flexcat/src/bin_unix/flexcat` |
| PATH | in `~/.bashrc`: `export PATH="$HOME/development/morphos/flexcat/src/bin_unix:$PATH"` |

**Nicht im BIGFOOT-Deb:** `flexcat` (Paketliste geprüft: 0 Treffer).

**AmigaGPT:** Symlinks im Repo-Root (weil Makefile relative `C_h.sd` / `C_c.sd` erwartet):

```bash
ln -sf ~/development/morphos/flexcat/src/sd/C_h.sd ~/development/morphos/AmigaGPT/
ln -sf ~/development/morphos/flexcat/src/sd/C_c.sd ~/development/morphos/AmigaGPT/
```

---

## BIGFOOT MorphOS-SDK (erledigt)

| Schritt | Details |
| ------- | ------- |
| Paket | `morphos-sdk_20230510-1_amd64.deb` von https://bigfoot.morphos-team.net/test/ |
| Installation | `sudo apt install ./morphos-sdk.deb` (in WSL ggf. `wsl -u root` wenn `sudo` hängt) |
| Compiler | `ppc-morphos-gcc` → **GCC 11.3.0**, Pfad `/usr/bin/ppc-morphos-gcc` |
| SDK-Root | **`/gg`** (nicht `/opt/amiga`) |

### Symlinks `/opt/amiga` → `/gg`

`Makefile.MorphOS` erwartet Bebbo-Layout unter `/opt/amiga`. Einmalig (root):

```bash
sudo mkdir -p /opt/amiga/ppc-morphos /opt/amiga/lib/gcc/ppc-morphos
sudo ln -sfn /gg/os-include      /opt/amiga/os-include
sudo ln -sfn /gg/includestd      /opt/amiga/includestd
sudo ln -sfn /gg/include         /opt/amiga/ppc-morphos/include
sudo ln -sfn /gg/ppc-morphos/lib /opt/amiga/ppc-morphos/lib
sudo ln -sfn /gg/lib             /opt/amiga/lib
sudo ln -sfn /gg/lib/gcc/ppc-morphos/11.3.0 /opt/amiga/lib/gcc/ppc-morphos/11.2.0
```

*(Makefile nennt `GCCLIBDIR` …/11.2.0/, Paket liefert 11.3.0 — Symlink reicht.)*

**Es wird kein zweiter Compiler installiert** — nur dieses SDK + Symlinks.

---

## AmigaSDK-gcc (erledigt)

| Schritt | Details |
| ------- | ------- |
| Clone | `~/development/morphos/AmigaSDK-gcc` |
| Nutzung | `Makefile.MorphOS` bindet `morphos/sdk/` automatisch ein (`AMIGA_SDK_EXTRA`) — **kein** `sudo cp` nötig |
| Doku | [MORPHOS-SDK-ERGAENZUNGEN.md](MORPHOS-SDK-ERGAENZUNGEN.md) — exakte Header/Libs |

---

## MorphOS-Build (erledigt, nativ ohne Docker)

```bash
cd ~/development/morphos/AmigaGPT
export PATH="$HOME/development/morphos/flexcat/src/bin_unix:$PATH"
make -f Makefile.MorphOS
make -f Makefile.MorphOS daemon
```

| Ausgabe | Pfad |
| ------- | ---- |
| Hauptprogramm | `out/AmigaGPT_MorphOS` (~4,9 MB) |
| Daemon | `out/AmigaGPTD_MorphOS` (~4,8 MB) |

**Test-Kopie für MorphOS-Maschine:** `Z:\morphos\out-crosscompile\` (Binaries + `.info` + `rexx/`, `devs/`).

---

## Dokumentation im Repo (Commits auf `scintilla`)

| Datei | Zweck |
| ----- | ----- |
| `docs/SCINTILLA-ARCHITECTURE.md` | Architekturplan Scintilla / Streaming |
| `docs/GIT-FORK-WORKFLOW.md` | Fork, upstream, Branch-Hinweis |
| `docs/HANDLUNGSANWEISUNG-GIT.md` | Git-Regeln Mensch + Agent |
| `docs/BUILD-MORPHOS-WSL.md` | Schritt-für-Schritt Build-Anleitung |
| `docs/MORPHOS-SDK-ERGAENZUNGEN.md` | Welche SDK-Ergänzungen genau nötig sind |
| `docs/WSL-SETUP-STATUS.md` | dieser Ist-Stand |
| `docs/README.md` | Index aller Docs |
| `AGENTS.md` | Kurzinfo für Cursor-Agenten |
| `.cursor/rules/git-branch-policy.mdc` | Cursor: nie auf `master` committen |

---

## Offene Punkte (Checkliste)

- [x] `AmigaSDK-gcc` geklont; Makefile nutzt `morphos/sdk` automatisch
- [x] `make -f Makefile.MorphOS` + `daemon` → `out/AmigaGPT_MorphOS`, `out/AmigaGPTD_MorphOS`
- [ ] `~/.bashrc`: FlexCat-PATH dauerhaft
- [x] Dokumentation: kanonischer Pfad nur noch WSL (`~/development/morphos/AmigaGPT`)
- [ ] Cursor-Workspace dauerhaft auf `\\wsl$\Debian\home\weimer\development\morphos\AmigaGPT` (nicht `C:\Users\xbox\cursorWorkspace\AmigaGPT`)
- [ ] `git push origin scintilla` (lokale Doku-Commits)
- [ ] Optional: `upstream` einmal `git fetch` + Merge in `scintilla`
- [ ] Optional: nach `.pot`/`.po`-Änderung einmal **`make catalog`** bzw. vollständiger Build, damit `AmigaGPT_cat.*` aktuell sind
- [x] MorphOS: Code-Viewer 7a/7b (Copy/Save, Quit mit offenem Viewer) — siehe [SCINTILLA-ARCHITECTURE.md](SCINTILLA-ARCHITECTURE.md)
- [ ] Optional: Stream-Recovery R1/R2 auf MorphOS (WANT_READ, Chat vs. `display_text`) — [STREAM-RECOVERY.md](STREAM-RECOVERY.md)
- [ ] **Optional (TODO):** Deploy in `package-morphos-cross.sh` zusätzlich/alternativ per **`smbclient`** (reines WSL, ohne `powershell.exe` / gemapptes `Z:`). Siehe [HANDLUNGSANWEISUNG-GIT.md](HANDLUNGSANWEISUNG-GIT.md) §8.

---

## Optional (TODO): Deploy per `smbclient` (WSL)

**Problem heute:** Deploy nutzt `powershell.exe` → `Z:\morphos\out-crosscompile\`. Das funktioniert nur, wenn Windows `Z:` (Share `\\hdsfgo4\share`) in **derselben Session** sieht und Credentials hat. Aus dem **WSL-Terminal** fehlt das oft (`DriveNotFound`); UNC ohne Login ebenfalls.

**Ziel:** LHA nach `//hdsfgo4/share/morphos/out-crosscompile/` direkt aus Bash kopieren (z. B. `smbclient put`), mit Credentials aus Umgebungsvariablen (nicht ins Repo committen).

**Beibehalten:** Größen-/MD5-Prüfung am Ziel, `DEPLOY-HISTORY.txt`, Exit 1 bei Deploy-Fehler, Fallback `DEPLOY=0` + manuell.

**Nicht ersetzen ohne Absprache:** PowerShell-Pfad kann als Fallback bleiben, wenn `Z:` in der Cursor-/Windows-Session verfügbar ist.

---

## Kurzreferenz Befehle

```bash
# SDK prüfen
ppc-morphos-gcc --version
ls -la /opt/amiga/

# FlexCat prüfen
which flexcat
flexcat -h

# Build
cd ~/development/morphos/AmigaGPT && make -f Makefile.MorphOS
```
