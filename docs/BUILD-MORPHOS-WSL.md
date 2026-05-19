# MorphOS Cross-Build unter WSL2 (Debian)

Anleitung zum Bauen von **AmigaGPT** für MorphOS auf **WSL2** mit **Debian 11/12**. Das Projekt nutzt `Makefile.MorphOS` und erwartet ein SDK unter `/opt/amiga/`.

Siehe auch: [GIT-FORK-WORKFLOW.md](GIT-FORK-WORKFLOW.md), [SCINTILLA-ARCHITECTURE.md](SCINTILLA-ARCHITECTURE.md).

---

## Übersicht

| Weg | Wann sinnvoll |
| --- | ------------- |
| **BIGFOOT `morphos-sdk` .deb** | Native Cross-Toolchain in WSL (empfohlen) |
| **`setup-cross-sdk.sh`** | SDK selbst bauen / andere Version |
| **`./build_morphos.sh` + Docker** | Vorkonfiguriertes Image, ohne natives SDK |

**Reihenfolge:** WSL2 → Debian-Pakete → **FlexCat** → MorphOS-SDK → ggf. AmigaSDK-gcc-Libs → AmigaGPT bauen.

---

## 1. WSL2 mit Debian

In PowerShell (Admin):

```powershell
wsl --install -d Debian
```

Nach Neustart prüfen, dass **VERSION 2** gilt:

```powershell
wsl -l -v
```

In der Debian-Shell:

```bash
sudo apt update && sudo apt upgrade -y
sudo apt install -y build-essential git wget curl make flex bison \
  gettext pkg-config
```

**Hinweis:** `flexcat` gibt es **nicht** in Debian-APT (und nicht im BIGFOOT-SDK-Paket, siehe Abschnitt 2).

**Debian 12 (Bookworm)** oder **11 (Bullseye)** auf **amd64** — passend zum offiziellen SDK-Paket.

---

## 2. FlexCat bauen (Pflicht für AmigaGPT)

AmigaGPT erzeugt beim Build `src/AmigaGPT_cat.c` und `src/AmigaGPT_cat.h` per **FlexCat** (nicht in `.git`, in `.gitignore`). Ohne `flexcat` im `PATH` scheitert `make -f Makefile.MorphOS`.

### Nicht enthalten

| Quelle | `flexcat`? |
| ------ | ---------- |
| `apt install flexcat` | **Nein** — Paket existiert in Debian nicht |
| `morphos-sdk_20230510-1_amd64.deb` (BIGFOOT) | **Nein** — nur Toolchain/SDK (`ppc-morphos-gcc`, …); ggf. `FlexLexer.h` (unrelated) |

### Quellcode holen

```bash
mkdir -p ~/development/morphos
cd ~/development/morphos
git clone https://github.com/adtools/flexcat.git
cd flexcat
```

### Bootstrap (wichtig beim ersten Build)

FlexCat will seine Katalog-Header beim Build mit `flexcat` neu erzeugen — das Programm existiert noch nicht. Dafür gibt es das Make-Ziel **`bootstrap`** (setzt Timestamps auf die mitgelieferten Dateien):

```bash
make bootstrap
make
```

**Typischer Fehler ohne `bootstrap`:**

```text
make[1]: flexcat: No such file or directory
make[1]: *** [Makefile:516: FlexCat_cat.h] Error 127
```

### Binary und PATH

Ergebnis (Linux/WSL, `OS=unix`):

```text
~/development/morphos/flexcat/src/bin_unix/flexcat
```

Prüfen:

```bash
~/development/morphos/flexcat/src/bin_unix/flexcat -h
```

Dauerhaft im `PATH` (Beispiel):

```bash
echo 'export PATH="$HOME/development/morphos/flexcat/src/bin_unix:$PATH"' >> ~/.bashrc
source ~/.bashrc
which flexcat
```

Compiler-Warnungen (`-gstabs`, Format) beim FlexCat-Build sind unkritisch.

---

## 3. MorphOS Cross-SDK installieren

### Variante A: Debian-Paket (schnell)

Von [MorphZone / BIGFOOT](https://www.morphzone.org/) — May 2023 SDK als `.deb` für Debian amd64:

```bash
cd /tmp
wget 'https://bigfoot.morphos-team.net/test/morphos-sdk_20230510-1_amd64.deb'
sudo apt install ./morphos-sdk_20230510-1_amd64.deb
ppc-morphos-gcc --version
```

**BIGFOOT-Paket installiert nach `/gg`, nicht nach `/opt/amiga`.**  
`Makefile.MorphOS` erwartet Bebbo-Layout unter `/opt/amiga`. Einmalig Symlinks anlegen (als root):

```bash
sudo mkdir -p /opt/amiga/ppc-morphos /opt/amiga/lib/gcc/ppc-morphos
sudo ln -sfn /gg/os-include      /opt/amiga/os-include
sudo ln -sfn /gg/includestd      /opt/amiga/includestd
sudo ln -sfn /gg/include         /opt/amiga/ppc-morphos/include
sudo ln -sfn /gg/ppc-morphos/lib /opt/amiga/ppc-morphos/lib
sudo ln -sfn /gg/lib             /opt/amiga/lib
# Makefile nennt 11.2.0, Paket liefert 11.3.0:
sudo ln -sfn /gg/lib/gcc/ppc-morphos/11.3.0 /opt/amiga/lib/gcc/ppc-morphos/11.2.0
```

### Variante B: `setup-cross-sdk.sh`

Skript **1.11** zum Aufsetzen der Cross-Umgebung auf Unix-Systemen (primär getestet auf Debian/Linux). Details und Updates im MorphZone-Thread zum Script.

---

## 4. Installation prüfen

```bash
which flexcat
which ppc-morphos-gcc
ppc-morphos-gcc --version
ls -la /opt/amiga/
ls /opt/amiga/lib/gcc/ppc-morphos/
```

`Makefile.MorphOS` verwendet u. a.:

| Variable | Standardpfad |
| -------- | ------------ |
| `CC` | `ppc-morphos-gcc` |
| `SDKLIBDIR` | `/opt/amiga/ppc-morphos/lib` |
| `LIBDIR` | `/opt/amiga/lib` |
| `SDKDIR` | `/opt/amiga/os-include` |
| `NETINCDIR` | `/opt/amiga/includestd` |
| `INCDIR` | `/opt/amiga/ppc-morphos/include` |
| `GCCLIBDIR` | `/opt/amiga/lib/gcc/ppc-morphos/11.2.0/` |

**Wichtig:** Wenn unter `ppc-morphos/` eine andere GCC-Version liegt als `11.2.0`, in `Makefile.MorphOS` die Zeile `GCCLIBDIR` anpassen.

---

## 5. Zusätzliche SDKs (Pflicht für AmigaGPT)

Aus https://github.com/sacredbanana/AmigaSDK-gcc — u. a.:

- **json-c**, **ssl**, **crypto** (Link)
- **SDI** / `SDI_hook.h` und weitere Dev-Includes

Nach `/opt/amiga` bzw. in die Include-Pfade legen, wie im AmigaSDK-gcc-Repo beschrieben.

Typische Fehler ohne diesen Schritt:

- `fatal error: SDI_hook.h: No such file or directory`
- `cannot find -ljson-c` / `-lssl`

**FlexCat `C_h.sd`:** Das Makefile nutzt relative Pfade; bei Fehler `cannot open … C_h.sd` aus dem FlexCat-Repo kopieren oder verlinken:

```bash
ln -sf ~/development/morphos/flexcat/src/sd/C_h.sd ~/development/AmigaGPT/
ln -sf ~/development/morphos/flexcat/src/sd/C_c.sd ~/development/AmigaGPT/
```

---

## 6. Quellcode (WSL-Home)

**Empfohlen** (schnellere Builds, ext4):

```bash
mkdir -p ~/development
cd ~/development
git clone https://github.com/weiseb78/AmigaGPT.git
cd AmigaGPT
git checkout scintilla
```

Von Windows rüberkopieren (falls lokale Commits noch nicht gepusht):

```bash
cp -a /mnt/c/Users/xbox/cursorWorkspace/AmigaGPT/. ~/development/AmigaGPT/
```

Von Windows aus: `\\wsl$\Debian\home\<user>\AmigaGPT`

**Alternativ** Windows-Pfad (langsamer):

```bash
cd /mnt/c/Users/xbox/cursorWorkspace/AmigaGPT
```

---

## 7. Bauen

```bash
make -f Makefile.MorphOS
```

Optional Daemon:

```bash
make -f Makefile.MorphOS daemon
```

Ausgabe:

- `out/AmigaGPT_MorphOS`
- `out/AmigaGPTD_MorphOS` (Daemon)

Debug:

```bash
DEBUG=1 make -f Makefile.MorphOS
```

Clean:

```bash
make -f Makefile.MorphOS clean
```

---

## 8. Docker-Alternative

```bash
./build_morphos.sh
```

Nutzt Image `sacredbanana/amiga-compiler:ppc-morphos`. Unter WSL: **Docker Desktop** mit WSL2-Backend aktivieren.

Clean-Build:

```bash
CLEAN=1 ./build_morphos.sh
```

---

## Häufige Probleme

| Symptom | Maßnahme |
| ------- | -------- |
| `Unable to locate package flexcat` | Normal — FlexCat aus Quellen bauen (Abschnitt 2), nicht per APT |
| `flexcat: No such file or directory` beim FlexCat-Build | `make bootstrap` vor `make` im FlexCat-Repo |
| `ppc-morphos-gcc: not found` | SDK-Paket nicht installiert oder `PATH` prüfen |
| `SDI_hook.h: No such file` | [AmigaSDK-gcc](https://github.com/sacredbanana/AmigaSDK-gcc) installieren |
| `cannot open … C_h.sd` | Symlinks zu `flexcat/src/sd/C_*.sd` im AmigaGPT-Root (siehe Abschnitt 5) |
| SDK unter `/gg`, Build sucht `/opt/amiga` | Symlinks aus Abschnitt 3 anlegen |
| `cannot find -ljson-c` / `-lssl` | [AmigaSDK-gcc](https://github.com/sacredbanana/AmigaSDK-gcc) nachinstallieren |
| Linker sucht falsche GCC-Version | `GCCLIBDIR` in `Makefile.MorphOS` anpassen |
| Katalog-Fehler bei AmigaGPT | `which flexcat`; PATH auf `bin_unix/flexcat` setzen |
| Sehr langsamer Build auf `/mnt/c` | Repo nach `~/` klonen |

---

## OS3 / OS4

Für **AmigaOS 3** und **4** siehe `README.md` (Bebbo `amiga-gcc`, `adtools`, Docker `build_os3.sh` / `build_os4.sh`). Diese Anleitung gilt nur für **MorphOS**.
