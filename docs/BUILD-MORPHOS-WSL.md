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

---

## 1. WSL2 mit Debian

In PowerShell (Admin):

```powershell
wsl --install -d Debian
```

In der Debian-Shell:

```bash
sudo apt update && sudo apt upgrade -y
sudo apt install -y build-essential git wget curl make flex bison \
  gettext flexcat pkg-config
```

**Debian 12 (Bookworm)** oder **11 (Bullseye)** auf **amd64** — passend zum offiziellen SDK-Paket.

---

## 2. MorphOS Cross-SDK installieren

### Variante A: Debian-Paket (schnell)

Von [MorphZone / BIGFOOT](https://www.morphzone.org/) — May 2023 SDK als `.deb` für Debian amd64:

```bash
cd /tmp
wget 'https://bigfoot.morphos-team.net/test/morphos-sdk_20230510-1_amd64.deb'
sudo apt install ./morphos-sdk_20230510-1_amd64.deb
```

### Variante B: `setup-cross-sdk.sh`

Skript **1.11** zum Aufsetzen der Cross-Umgebung auf Unix-Systemen (primär getestet auf Debian/Linux). Details und Updates im MorphZone-Thread zum Script.

---

## 3. Installation prüfen

```bash
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

## 4. Zusätzliche Bibliotheken (Link)

`Makefile.MorphOS` linkt u. a. **json-c**, **ssl**, **crypto**. Diese liegen oft im Maintainer-SDK-Bundle:

- https://github.com/sacredbanana/AmigaSDK-gcc  

Bibliotheken und Includes wie dort beschrieben nach `/opt/amiga` legen. Typischer Linkerfehler ohne diese Schritte: `cannot find -ljson-c` oder `-lssl`.

---

## 5. Quellcode

**Empfohlen** (schnellere Builds, ext4):

```bash
cd ~
git clone https://github.com/weiseb78/AmigaGPT.git
cd AmigaGPT
```

Von Windows aus: `\\wsl$\Debian\home\<user>\AmigaGPT`

**Alternativ** Windows-Pfad (langsamer):

```bash
cd /mnt/c/Users/xbox/cursorWorkspace/AmigaGPT
```

---

## 6. Bauen

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

## 7. Docker-Alternative

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
| `ppc-morphos-gcc: not found` | SDK-Paket nicht installiert oder `PATH` prüfen |
| `cannot find -ljson-c` / `-lssl` | [AmigaSDK-gcc](https://github.com/sacredbanana/AmigaSDK-gcc) nachinstallieren |
| Linker sucht falsche GCC-Version | `GCCLIBDIR` in `Makefile.MorphOS` anpassen |
| `flexcat` / Katalog-Warnungen | `sudo apt install flexcat gettext` |
| Sehr langsamer Build auf `/mnt/c` | Repo nach `~/` klonen |

---

## OS3 / OS4

Für **AmigaOS 3** und **4** siehe `README.md` (Bebbo `amiga-gcc`, `adtools`, Docker `build_os3.sh` / `build_os4.sh`). Diese Anleitung gilt nur für **MorphOS**.
