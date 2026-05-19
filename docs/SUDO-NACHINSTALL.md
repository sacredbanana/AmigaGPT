# Einmalig mit sudo (WSL/Debian)

Wenn der Agent oder Build-Skripte an **Permission denied** / **sudo password** scheitern, diese Schritte **einmal** in der Debian-WSL-Shell ausführen.

---

## 1. Paketierung (empfohlen)

Für **ein Archiv** statt vieler Einzeldateien nach `Z:\morphos\…`:

```bash
sudo apt update
sudo apt install -y jlha-utils zip
```

| Paket | Zweck |
|-------|--------|
| **jlha-utils** | `.lha` erzeugen (klassisches Amiga-Format) — Befehl `jlha` |
| **zip** | Fallback `.zip`, falls kein LHA-Tool |

Danach:

```bash
cd ~/development/morphos/AmigaGPT
chmod +x package-morphos-cross.sh
./package-morphos-cross.sh

# optional: Skript nach tools/ (wenn root-Rechte gefixt)
# sudo chown -R "$USER:$USER" tools && mv package-morphos-cross.sh tools/
```

Erzeugt u. a. `out/AmigaGPT-MorphOS-cross.lha` (oder `.zip`) und kopiert nach `Z:\morphos\out-crosscompile\`.

---

## 2. Dateirechte (optional)

Falls `tools/` root gehört und Skripte nicht schreibbar sind:

```bash
sudo chown -R "$USER:$USER" ~/development/morphos/AmigaGPT/tools
```

---

## 3. AmigaSDK nach /opt/amiga (optional)

**Nicht mehr nötig**, wenn `Makefile.MorphOS` AmigaSDK-gcc automatisch einbindet (`AMIGA_SDK_EXTRA`).

Falls du das alte Bebbo-Layout ohne Makefile-Hack willst:

```bash
sudo cp -a ~/development/morphos/AmigaSDK-gcc/morphos/sdk/. /opt/amiga/
test -f /opt/amiga/ppc-morphos/include/SDI_hook.h && echo OK
```

---

## 4. BIGFOOT-Symlinks (falls noch nicht gesetzt)

Nur wenn `/opt/amiga` noch leer ist — siehe [BUILD-MORPHOS-WSL.md](BUILD-MORPHOS-WSL.md):

```bash
sudo mkdir -p /opt/amiga/ppc-morphos /opt/amiga/lib/gcc/ppc-morphos
sudo ln -sfn /gg/os-include      /opt/amiga/os-include
sudo ln -sfn /gg/includestd      /opt/amiga/includestd
sudo ln -sfn /gg/include         /opt/amiga/ppc-morphos/include
sudo ln -sfn /gg/ppc-morphos/lib /opt/amiga/ppc-morphos/lib
sudo ln -sfn /gg/lib             /opt/amiga/lib
sudo ln -sfn /gg/lib/gcc/ppc-morphos/11.3.0 /opt/amiga/lib/gcc/ppc-morphos/11.2.0
```

---

## Kurz: was wirklich nötig ist

| Schritt | Pflicht? |
|---------|----------|
| `morphos-sdk` .deb (BIGFOOT) | ja — Compiler |
| FlexCat bauen | ja — Kataloge |
| AmigaSDK-gcc klonen | ja — SDI, json-c, ssl, MUI-Header |
| **jlha-utils** + package-Skript | empfohlen — ein Datei-Transfer |
| sudo `cp` nach `/opt/amiga` | nein (Makefile-Auto-Pfad) |
| Docker | nein |
