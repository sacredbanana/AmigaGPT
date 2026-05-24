# Versionsnummern (Fork)

AmigaGPT nutzt **eine** offizielle Versionsnummer auf allen Zielplattformen:

```text
MAJOR.MINOR.BUILD_NUMBER
```

Beispiel: **`2.18.8556`**

MorphOS-`$VER` folgt dem [Style Guide](https://library.morph.zone/Style_Guide#The_Correct_Version_String):

```text
$VER: AmigaGPT <Version>.<Revision> (DD.MM.YYYY) © <Jahr> <Autor>
```

Dabei ist **Version** = `MAJOR.MINOR`, **Revision** = `BUILD_NUMBER` (dieselbe dritte Ziffer wie in der Anzeige).

## Bedeutung

| Teil | Quelle | Beispiel | Wann ändern |
|------|--------|----------|-------------|
| **Major / Minor** | `APP_VERSION_MAJOR`, `APP_VERSION_MINOR` in `src/version.h` | `2.18` | manuell bei Release-Linie (z. B. Fork-Meilenstein 2.18) |
| **Build (Revision)** | `BUILD_NUMBER` in `src/version.h` | `8556` | automatisch bei jedem `make -f Makefile.MorphOS` (`version-bump`) |
| **PATCH** | `APP_VERSION_PATCH` in `src/version.h` | `0` | **nur** upstream-Merge-Kompatibilität — **nicht** in UI/`$VER` |
| **Datum** | `BUILD_DATE` (Compile-Flag) | `24.05.2026` | automatisch pro Build |
| **©** | `APP_COPYRIGHT` | … | bei Bedarf |

**Nicht** zurücksetzen: Bei Sprung von 2.17 auf 2.18 bleibt `BUILD_NUMBER` monoton (8556 → 8557 …), damit ältere Builds nicht „neuer“ wirken.

## Anzeige

- About, MUI `Application_Version`, Daemon-Log: `APP_VERSION` = `2.18.8556`
- `Version` auf MorphOS / `$VER`-Segment: gleiche Zahl

## Code

- `src/version.h` — `APP_VER_STRING_AMIGAGPT`, `APP_VER_STRING_AMIGAGPTD`
- `main.c` — Segment `version`
- `gui.c` — `MUIA_Application_Version`

## MorphOS Cross-Build vs. klassisches Amiga-Makefile

| Makefile | `BUILD_NUMBER` hochzählen | Getestet im Fork |
|----------|---------------------------|------------------|
| **`Makefile.MorphOS`** | **einmal** pro `make all` (`version-bump` vor dem Link) | ja (WSL Cross-Build) |
| **`Makefile`** / **`Makefile.OS4`** | bei **jedem** Kompilieren einer `.c`-Datei (`sed` in den Objekt-Regeln) | nein — Verhalten bewusst **unverändert** gelassen |

Klassische Amiga-Builds können in **einem** Lauf die Revision mehrfach erhöhen; MorphOS nicht. Anzeige-Format (`MAJOR.MINOR.BUILD_NUMBER`) ist trotzdem identisch. Native Amiga-Builds werden im Fork nicht aktiv geprüft — Makefile nicht anfassen, bis jemand gezielt testet.

## Paket & Deploy-Protokoll

`./package-morphos-cross.sh` schreibt ins Archiv `PACKAGE-BUILDINFO.txt` (`full_version`, `git`, Zeiten, **MD5** des Binaries — gleiches Format wie MorphOS Ambient „Datenbank“).

Nach **erfolgreichem** Deploy nach `Z:\morphos\out-crosscompile\` (Größe + MD5 der LHA):

- Datei **`DEPLOY-HISTORY.txt`** im selben Ordner
- Letzte **10** erfolgreiche Deploys (neueste unten)
- Spalten: `version | git | build_utc | copy_utc | lha_md5`

Nur bei verifiziertem Kopieren — kein Eintrag bei `DEPLOY=0` oder Deploy-Fehler.
