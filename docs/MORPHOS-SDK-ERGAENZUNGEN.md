# MorphOS-Build: SDK-Ergänzungen (genau)

Dokumentation der **zusätzlichen** Komponenten, die für einen nativen Cross-Build von AmigaGPT unter WSL **neben** dem BIGFOOT `morphos-sdk` nötig sind. **Kein Docker**, kein zweiter Compiler.

**Stand:** 2026-05-19 · erfolgreicher Build mit `make -f Makefile.MorphOS` auf Branch `scintilla`.

---

## Übersicht: drei Bausteine

| # | Komponente | Quelle | Zweck |
|---|------------|--------|--------|
| 1 | **BIGFOOT `morphos-sdk`** | `.deb` → `/gg` | `ppc-morphos-gcc`, MorphOS-Basis-Header/Libs |
| 2 | **FlexCat** (selbst gebaut) | `morphos/flexcat` | Katalog-Quellen `AmigaGPT_cat.c` / `.h` |
| 3 | **AmigaSDK-gcc** `morphos/sdk` | `morphos/AmigaSDK-gcc` | Header/Libs, die in `/gg` fehlen oder zu alt sind |

`Makefile.MorphOS` erkennt (3) automatisch, wenn  
`~/development/morphos/AmigaSDK-gcc/morphos/sdk/ppc-morphos/include/SDI_hook.h` existiert, und setzt Include-/Lib-Pfade auf dieses Verzeichnis (siehe Block `AMIGA_SDK_EXTRA` im Makefile).

---

## Was BIGFOOT liefert — und was fehlt

BIGFOOT installiert u. a.:

- Compiler: `/usr/bin/ppc-morphos-gcc` (GCC 11.3.x)
- SDK-Root: **`/gg`** (Standard-Suchpfade des Compilers: `/gg/include`, `/gg/os-include`, …)
- Symlinks nach **`/opt/amiga`** (für Makefiles im Bebbo-Layout)

**Ohne AmigaSDK-gcc** schlägt der Build u. a. fehl mit:

| Fehler | Ursache |
|--------|---------|
| `fatal error: SDI_hook.h: No such file or directory` | SDI nur im Zusatz-SDK |
| `MUIA_Aboutbox_URL undeclared` | neuere `Aboutbox_mcc.h` nur im Zusatz-SDK (`/gg` zu alt) |
| `cannot find -ljson-c` / `-lssl` / `-lcrypto` | Link-Libs nur im Zusatz-SDK |

---

## AmigaSDK-gcc: konkret genutzte Ergänzungen

Repository: https://github.com/sacredbanana/AmigaSDK-gcc  
Pfad im Workspace: `~/development/morphos/AmigaSDK-gcc/morphos/sdk/`

### Header (Compile-Zeit)

| Header / API | Pfad im SDK | Verwendung in AmigaGPT |
|--------------|-------------|------------------------|
| **SDI** | `ppc-morphos/include/SDI_hook.h` (+ SDI-Makros) | GUI-Fenster: `MakeHook`, MUI-SDI in vielen `src/*Window.c`, `menu.c`, `gui.c`, … |
| **json-c** | `include/json-c/json.h` (über `-I…/os-include` bzw. Compiler-Pfade) | `openai.c`, `config.c`, `MainWindow.c`, `gui.c`, `ARexx.c`, … |
| **OpenSSL (AmiSSL)** | `openssl/*.h` (Link über `-lssl -lcrypto`) | `openai.c` (HTTPS) |
| **codesets.library** | `ppc-morphos/include/libraries/codesets.h`, `proto/codesets.h` | `openai.h`, `config.h`, `gui.c` (UTF-8 / Zeichensätze) |
| **guigfx** | `os-include/guigfx/guigfx.h`, `proto/guigfx.h` | `gui.h` (Bildvorschau) |
| **MUI Aboutbox (neu)** | `os-include/mui/Aboutbox_mcc.h` | `AboutAmigaGPTWindow.c`, `gui.c` — u. a. `MUIA_Aboutbox_URL`, `MUIA_Aboutbox_URLText` (in `/gg` nicht vorhanden) |
| Weitere MUI-MCCs | z. B. `mui/Busy_mcc.h`, `BetterString_mcc.h`, `TextEditor_mcc.h` | über `os-include` / bestehende MUI-Includes |

**Wichtig:** Nur `os-include` und `ppc-morphos/include` aus dem Zusatz-SDK per `-I` vor `/gg` legen — **nicht** blind `sdk/include` voranstellen (sonst Konflikte z. B. bei `netdb.h` / `socket_protos.h`).

### Libraries (Link-Zeit, `Makefile.MorphOS` → `LDFLAGS`)

| Library | Typischer Pfad | Makefile |
|---------|----------------|----------|
| **libjson-c** | `ppc-morphos/lib/libjson-c.a` | `-ljson-c` |
| **libssl** | `ppc-morphos/lib/libssl.a` | `-lssl` |
| **libcrypto** | (mit ssl-Paket) | `-lcrypto` |
| **libm**, **libatomic** | SDK / GCC | `-lm`, `-latomic` |
| **GCC-Runtime** | `lib/gcc/ppc-morphos/11.2.0/` | `-L$(GCCLIBDIR)` |

AmiSSL wird zur Laufzeit auf MorphOS als **Bibliothek auf dem Zielsystem** erwartet (nicht statisch vollständig ins Binary eingebettet wie ein Linux-.so); der Cross-Build linkt gegen die SDK-Stub-/Import-Libs.

### FlexCat (separat, nicht Teil von AmigaSDK-gcc)

| Datei | Zweck |
|-------|--------|
| `flexcat` Binary in `PATH` (`morphos/flexcat/src/bin_unix/`) | erzeugt `src/AmigaGPT_cat.c`, `src/AmigaGPT_cat.h`, `.catalog` |
| Symlinks `C_h.sd`, `C_c.sd` im AmigaGPT-Root → `flexcat/src/sd/` | FlexCat-Syntax-Definitionen für das Makefile |

### Katalog-Übersetzung (Build-Hinweis)

In `catalogs/german/deutsch.po` müssen Anführungszeichen in `msgstr` escaped sein (`\"…\"`), sonst bricht FlexCat/`msgmerge` mit `keyword "…" unknown` ab.

---

## Verzeichnis-Mapping im Makefile (automatisch)

Wenn `AMIGA_SDK_EXTRA` gesetzt ist (Default: `$(HOME)/development/morphos/AmigaSDK-gcc/morphos/sdk`):

| Makefile-Variable | Zeigt auf |
|-------------------|-----------|
| `SDKDIR` | `$(AMIGA_SDK_EXTRA)/os-include` |
| `INCDIR` | `$(AMIGA_SDK_EXTRA)/ppc-morphos/include` |
| `NETINCDIR` | `$(AMIGA_SDK_EXTRA)/includestd` |
| `LIBDIR` | `$(AMIGA_SDK_EXTRA)/lib` |
| `SDKLIBDIR` | `$(AMIGA_SDK_EXTRA)/ppc-morphos/lib` |
| `GCCLIBDIR` | `$(AMIGA_SDK_EXTRA)/lib/gcc/ppc-morphos/11.2.0/` |
| `CCFLAGS` | zusätzlich `-I$(SDKDIR) -I$(INCDIR)` |

Alternative (optional, einmalig mit root): gesamtes `morphos/sdk/` nach `/opt/amiga` kopieren — dann kann der Makefile-Block entfallen, sofern Pfade konsistent zu `/gg` sind.

---

## Einmal-Setup (Kurz)

```bash
# 1) BIGFOOT morphos-sdk (.deb), Symlinks /opt/amiga → /gg (siehe BUILD-MORPHOS-WSL.md)

# 2) FlexCat
cd ~/development/morphos/flexcat && make bootstrap && make
export PATH="$HOME/development/morphos/flexcat/src/bin_unix:$PATH"
ln -sf ~/development/morphos/flexcat/src/sd/C_h.sd ~/development/morphos/AmigaGPT/
ln -sf ~/development/morphos/flexcat/src/sd/C_c.sd ~/development/morphos/AmigaGPT/

# 3) AmigaSDK-gcc
git clone --depth 1 https://github.com/sacredbanana/AmigaSDK-gcc.git \
  ~/development/morphos/AmigaSDK-gcc

# 4) Bauen
cd ~/development/morphos/AmigaGPT
make -f Makefile.MorphOS
make -f Makefile.MorphOS daemon
```

Ausgabe: `out/AmigaGPT_MorphOS`, `out/AmigaGPTD_MorphOS`.

---

## Test auf echter MorphOS-Hardware

**Empfohlen:** ein Paket statt Einzeldateien kopieren (Begriff **Paketieren** = inkl. erfolgreicher Bereitstellung unter **`Z:\morphos\out-crosscompile\`**, siehe [HANDLUNGSANWEISUNG-GIT.md](HANDLUNGSANWEISUNG-GIT.md) Abschnitt 8):

```bash
# einmalig (sudo): siehe docs/SUDO-NACHINSTALL.md
sudo apt install -y jlha-utils zip

cd ~/development/morphos/AmigaGPT
./package-morphos-cross.sh          # BUILD=1 Standard, DEPLOY=1: .lha + Z:
```

Ausgabe bei Erfolg: `out/package-morphos/` + `out/AmigaGPT-MorphOS-cross.lha` (oder `.zip`) **und** Kopie nach `Z:\morphos\out-crosscompile\`. Schlägt der Deploy fehl, endet das Skript mit **Exit 1**. Anleitung im Paket: `INSTALL-MORPHOS-CROSSBUILD.txt`.

Manuell geht weiterhin das Kopieren einzelner Dateien nach `Z:\morphos\out-crosscompile\` (ersetzt dann den automatischen Deploy-Schritt, sofern das Ergebnis dort liegt).

Auf MorphOS zusätzlich Laufzeit-Voraussetzungen aus dem AmigaGPT-README (MUI, AmiSSL, codesets.library, ggf. guigfx MCC, …).

---

## Siehe auch

- [BUILD-MORPHOS-WSL.md](BUILD-MORPHOS-WSL.md) — vollständige WSL-Anleitung
- [WSL-SETUP-STATUS.md](WSL-SETUP-STATUS.md) — Checkliste Ist-Stand
