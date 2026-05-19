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

## Verzeichnislayout (WSL)

```text
~/development/
  morphos/
    flexcat/              ← gebaut (FlexCat 2.18)
  AmigaGPT/               ← Arbeitskopie für Build (von Windows kopiert)
```

**Parallel (Windows / Cursor):**

```text
C:\Users\xbox\cursorWorkspace\AmigaGPT   ← Git-Repo, Branch scintilla, Doku-Commits
```

**Empfehlung:** Cursor-Workspace auf WSL umstellen:

```text
\\wsl$\Debian\home\weimer\development\AmigaGPT
```

Bis dahin: nach Doku-Commits ggf. `cp -a` von Windows nach WSL oder `git pull` nach Push.

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
ln -sf ~/development/morphos/flexcat/src/sd/C_h.sd ~/development/AmigaGPT/
ln -sf ~/development/morphos/flexcat/src/sd/C_c.sd ~/development/AmigaGPT/
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

## AmigaSDK-gcc (noch offen)

**Kein Compiler**, sondern **SDK-Zusatzpaket** (Header, Libraries, FlexCat-SD) für GCC-Builds:

https://github.com/sacredbanana/AmigaSDK-gcc

Für MorphOS listet das Repo u. a.: **json-c**, **codesets**, **guigfx**, FlexCat-SD.  
AmigaGPT braucht zusätzlich z. B. **`SDI_hook.h`** (SDI eher im OS4-Teil des Pakets).

Installation laut README: Inhalt von `sdk/` ins Dev-Layout legen (bei uns: passend zu `/opt/amiga` / `/gg`).

**Alternative:** Docker `sacredbanana/amiga-compiler:ppc-morphos` + `./build_morphos.sh` (alles vorkonfiguriert).

---

## Erster Build-Versuch AmigaGPT

```bash
cd ~/development/AmigaGPT
export PATH="$HOME/development/morphos/flexcat/src/bin_unix:/usr/bin:/bin"
make -f Makefile.MorphOS
```

| Phase | Ergebnis |
| ----- | -------- |
| Kataloge / flexcat | läuft (mit Warnungen bei einzelnen `.po`) |
| Kompilieren | **Abbruch:** `fatal error: SDI_hook.h: No such file or directory` |

→ Nächster Schritt: **AmigaSDK-gcc** (oder Docker-Image) einspielen.

---

## Dokumentation im Repo (Commits auf `scintilla`)

| Datei | Zweck |
| ----- | ----- |
| `docs/SCINTILLA-ARCHITECTURE.md` | Architekturplan Scintilla / Streaming |
| `docs/GIT-FORK-WORKFLOW.md` | Fork, upstream, Branch-Hinweis |
| `docs/HANDLUNGSANWEISUNG-GIT.md` | Git-Regeln Mensch + Agent |
| `docs/BUILD-MORPHOS-WSL.md` | Schritt-für-Schritt Build-Anleitung |
| `docs/WSL-SETUP-STATUS.md` | dieser Ist-Stand |
| `docs/README.md` | Index aller Docs |
| `AGENTS.md` | Kurzinfo für Cursor-Agenten |
| `.cursor/rules/git-branch-policy.mdc` | Cursor: nie auf `master` committen |

---

## Offene Punkte (Checkliste)

- [ ] `AmigaSDK-gcc` installieren **oder** Docker-Build testen
- [ ] `make -f Makefile.MorphOS` erfolgreich bis `out/AmigaGPT_MorphOS`
- [ ] `~/.bashrc`: FlexCat-PATH dauerhaft
- [ ] Cursor-Workspace auf `\\wsl$\Debian\home\weimer\development\AmigaGPT`
- [ ] `git push origin scintilla` (lokale Doku-Commits)
- [ ] Optional: `upstream` einmal `git fetch` + Merge in `scintilla`

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
cd ~/development/AmigaGPT && make -f Makefile.MorphOS
```
