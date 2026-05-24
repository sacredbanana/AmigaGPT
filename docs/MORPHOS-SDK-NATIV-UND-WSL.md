# MorphOS-SDK: nativ (Referenz) vs. WSL (Cross-Build)

**Stand:** 2026-05-20 · Branch `scintilla`  
**Zweck:** Alles Schriftliche aus der SDK-/Scintilla-Diskussion an einem Ort — Arbeitsaufteilung, Verzeichnisse, `pack`, Scintilla-API, Phase-6-Stand, nächste Schritte.

Siehe auch: [MORPHOS-SDK-ERGAENZUNGEN.md](MORPHOS-SDK-ERGAENZUNGEN.md) (WSL-Ergänzungen), [SCINTILLA-ARCHITECTURE.md](SCINTILLA-ARCHITECTURE.md) (Phasen 6–13).

---

## 1. Arbeitsaufteilung (umgekehrt zur ersten Annahme)

| Ort | Was vorhanden ist | Rolle für AmigaGPT |
|-----|-------------------|-------------------|
| **MorphOS (nativ)** | **Voll-SDK** nach Team-Installer (`sdk.pack` → `Guide/`, `Examples/`, `gg/`, …). Entwickler hat SDK installiert; Dateien können bei Bedarf bereitgestellt werden. | **Referenz und Wahrheit:** Guides, Autodocs, exakte Header, Laufzeit (`Scintilla.mcc`, `ttengine.library`), Test auf Hardware. |
| **WSL (Linux)** | BIGFOOT `morphos-sdk.deb` → nur `/gg` (Toolchain, Basis-Header/Libs). **Kein** vollständiges `Guide/` / `Examples/` im Deb. Zusatz: `AmigaSDK-gcc/morphos/sdk` + optional `morphos/docs`-Auszug. | **Cross-Build:** `make -f Makefile.MorphOS`, `package-morphos-cross.sh` → LHA nach `Z:\morphos\out-crosscompile\`. |

**Kurz:** Cross-Build braucht WSL weiterhin BIGFOOT + AmigaSDK-gcc. Das **native MorphOS-SDK** ersetzt das nicht auf WSL, sondern liefert Doku, Beispiele und Abgleich (z. B. `Scintilla.guide`, Lexer in Phase 11).

---

## 2. Inventar: `morphos/devfiles.txt`

`~/development/morphos/devfiles.txt` ist ein **Verzeichnislisting** des installierten SDK auf MorphOS (Datum im Listing: Mai 2026). Wurzel entspricht dem Layout nach **SDK Installer** / Team-`sdk.pack`, nicht nur `gg:`.

### 2.1 Top-Level (SDK-Wurzel)

| Verzeichnis / Datei | Inhalt (Auszug) |
|---------------------|-----------------|
| **`Guide/`** | Autodocs: u. a. `Scintilla.guide` (~396 KB), `ttengine.guide`, `Guide/includes/mui/Scintilla_mcc.h`, `Guide/includes/Scintilla/` |
| **`Examples/`** | u. a. `Examples/MUI/` (klassische MUI-Demos) — **kein** Scintilla-`.c` |
| **`Frameworks/`** | OB/MUI-Framework, `Frameworks/include/mui/MCCScintilla.h` |
| **`Docs/`**, **`Tools/`**, **`Libs/`**, **`Tutorials/`**, **`IndexFiles/`** | SDK-Dokumentation und Werkzeuge |
| **`gg/`** | GeekGadgets: Compiler, `os-include/`, `ppc-morphos/`, `home/`, Perl, … |
| **`gg.info`**, **`ValidateGG`**, **`SDK.readme`** | SDK-Metadaten / Validierung |

### 2.2 Scintilla-relevante Pfade (nativ)

| Pfad (relativ zur SDK-Wurzel bzw. unter `gg`) | Datei / Größe (Listing) | Hinweis |
|-----------------------------------------------|-------------------------|---------|
| `Guide/Scintilla.guide` | ~395712 B | Hauptdoku (Lexer, Styles, MUI) |
| `Guide/includes/mui/Scintilla_mcc.h` | 3142 B | Header (Guide-Baum; etwas größer als `gg/os-include`) |
| `Guide/includes/Scintilla/Scintilla.h` | 40702 B | Kern-API |
| `gg/os-include/mui/Scintilla_mcc.h` | 2857 B | Cross-Build-relevanter Pfad (entspricht AmigaSDK-gcc-Auszug) |
| `gg/os-include/Scintilla/` | Verzeichnis | `Scintilla.h`, `ScintillaWidget.h` |
| `Frameworks/include/mui/MCCScintilla.h` | 661 B | OB-Framework-Hülle |

### 2.3 TTEngine (nativ)

| Pfad | Inhalt |
|------|--------|
| `Guide/ttengine.guide` | Doku |
| `Guide/includes/…` / `gg/os-include/libraries/ttengine.h` | Header (mehrere Kopien im SDK) |
| Laufzeit | `ttengine.library` in `gui.c`; Guide: **`SCI_SETFONTQUALITY(1)`** für TTEngine-Unicode — siehe [VON-MORPHOS-SDK-SCINTILLA.md](VON-MORPHOS-SDK-SCINTILLA.md) |

### 2.4 Was **nicht** im SDK-Examples liegt

Unter `Examples/MUI/` nur Standarddemos (`MUI-Demo.c`, `Class1.c`, …). **Kein** offizielles Scintilla-Demo-`.c`. Referenzcode für Scintilla-MUI eher in **Drittanwendungen** (z. B. Flow Studio) oder im **Scintilla.mcc**-Paket unter `DEV:MUI` / morphos-storage.

---

## 3. `pack` — drei verschiedene Begriffe

| Begriff | Bedeutung |
|---------|-----------|
| **`sdk.pack`** | Inhalt von `sdk-20230510.lha` (MorphOS Team). **Kein LHA** — Entpacken nur mit **SDK Installer auf MorphOS**. Erzeugt u. a. `Guide/`, `Examples/`, Symlinks nach `gg:`. |
| **Eigenes `pack` (Setup)** | Vom Entwickler erwähnt: Vereinfachte Installation der **Entwicklungsumgebung auf MorphOS** (Symlinks, Besonderheiten). Nicht identisch mit `sdk.pack`; Details liegen in der lokalen MorphOS-Installation, nicht im WSL-Repo. |
| **`pack.h` (SDK-Header)** | Im Listing unter `Guide/includes/` und `gg/os-include/` — API zum Pack-Format (Archiv), **nicht** Git `objects/pack`. |

**WSL:** `sdk.pack` wird **nicht** in WSL entpackt; dort reichen BIGFOOT-Deb + `AmigaSDK-gcc` + optional Doku-Auszug unter `AmigaSDK-gcc/morphos/docs/`.

---

## 4. WSL-Ergänzungen (Kurzverweis)

Vollständige Tabelle: [MORPHOS-SDK-ERGAENZUNGEN.md](MORPHOS-SDK-ERGAENZUNGEN.md).

| # | Komponente | Zweck |
|---|------------|--------|
| 1 | BIGFOOT `morphos-sdk.deb` → `/gg` | `ppc-morphos-gcc`, Basis-SDK |
| 2 | FlexCat (`morphos/flexcat`) | Katalog `AmigaGPT_cat.c` / `.h` |
| 3 | `AmigaSDK-gcc/morphos/sdk` | SDI, json-c, AmiSSL, neuere MCC-Header, `Scintilla_mcc.h`, … |

`Makefile.MorphOS` setzt `AMIGA_SDK_EXTRA` auf (3), wenn `SDI_hook.h` dort existiert.

---

## 5. Scintilla Code-Viewer — technische Festlegung (Phase 6–7b)

### 5.1 Implementierte Dateien

| Datei | Rolle |
|-------|--------|
| `src/CodeBlocksScintilla.c` / `.h` | Wrapper um `MUIM_Scintilla_Command` → `SCI_*` |
| `src/CodeBlocksViewer.c` / `.h` | MorphOS: NList, Copy/Save, ASL, `codeBlocksViewerPrepareShutdown()` |
| `src/gui.c` | Fenster „View code blocks…“; MorphOS: `codeBlocksViewerPopulate()`; OS3/OS4: `build_conversation_codeblocks_utf8()` + NFloattext; `ttengine.library` unter `#ifdef __MORPHOS__` |
| `src/MainWindow.c` | `codeBlocksViewerSetAslParentWindow(mainWindow)` |
| `Makefile.MorphOS` | baut `CodeBlocksScintilla.c` + `CodeBlocksViewer.c`; Daemon **ohne** diese Dateien; **keine** extra `-l` für Scintilla |
| `Makefile` / `Makefile.OS4` | **ohne** Scintilla/Viewer — OS3/OS4: NFloattext + codesets |

### 5.2 `MUIM_Scintilla_Command` = `MUIA_Scintilla_dummy + 2`

Im öffentlichen Header `Scintilla_mcc.h` steht `struct MUIP_Scintilla_Command`, aber **kein** `#define MUIM_Scintilla_Command`.

Offizielle Slot-Reihenfolge im Header:

- `MUIA_Scintilla_ActiveEditor` = dummy **+1**
- *(Slot +2 = Command — nicht als `#define` exportiert)*
- `MUIA_Scintilla_Notify` = dummy **+3**
- `MUIM_Scintilla_AutoComplete` = dummy **+4**, …

**Verbindliche Implementierung** (bestätigt durch Laufzeit auf MorphOS, konsistent mit Header-Lücke):

```c
#define MUIM_Scintilla_Command (MUIA_Scintilla_dummy + 2)

struct MUIP_Scintilla_Command cmd;
cmd.MethodID = MUIM_Scintilla_Command;
cmd.iMessage = (LONG)iMessage;
cmd.wParam   = (LONG)wParam;
cmd.lParam   = (LONG)lParam;
DoMethodA(sci, (Msg)&cmd);
```

**Nicht verwenden:** `SciFnDirect`, falsche Method-ID (z. B. +9 geraten), Debug-`SCI_SETTEXT` mit `"ok"` (zeigte Text, hing danach).

### 5.3 UTF-8 und Text setzen

- `SCI_SETCODEPAGE` mit `SC_CP_UTF8`
- `SCI_SETTEXT` mit **Roh-UTF-8** — **kein** `CodesetsUTF8ToStr` im Code-Viewer
- Fenster **offen**, dann setzen; danach read-only (`SCI_SETREADONLY`)
- Datenquelle MorphOS: `AICodeBlock.raw_code` des aktiven Blocks in `CodeBlocksViewer.c` (OS3/OS4 weiterhin `build_conversation_codeblocks_utf8()` in `gui.c`)

### 5.4 Laufzeit auf MorphOS

- **Scintilla.mcc** (MUI Custom Class) — nicht statisch gelinkt
- **ttengine.library** (`gui.c`) + **`SCI_SETFONTQUALITY(1)`** in `CodeBlocksScintilla.c` (TTEngine-Rendering laut Guide)

### 5.5 Code-Viewer-Status (Definition of Done, Teil)

| Kriterium | Status (2026-05-24) |
|-----------|---------------------|
| Menü „View code blocks…“ + Scintilla + NList (7a) | **Erledigt** (MorphOS) |
| Copy/Save UTF-8 + System-Codeset (7b) | **Erledigt** (MorphOS, inkl. Quit mit offenem Viewer) |
| Cross-Build `Makefile.MorphOS` + Daemon ohne Viewer | **Erledigt** |
| Stream/Chat-Sync, WANT_READ (Recovery R1–R2) | **Offen** — [STREAM-RECOVERY.md](STREAM-RECOVERY.md) |
| String-Safety (Phase 8) | **In Arbeit** — [PHASE-8-STRING-SAFETY.md](PHASE-8-STRING-SAFETY.md) (8.1); R2-Rest dokumentiert |
| Logs (Phase 9) | **Offen** |
| Vollständiges DoD Phase 10 (formal) | **Offen** |

Chat-Hauptfenster bleibt **NFloattext** bis Phase 12.

---

## 6. Workspace-Kopie aus MorphOS-SDK

**Kopiert (2026-05-20):** `~/development/morphos/VonMorphosSDK/`

| Datei | Format | Doku |
|-------|--------|------|
| `Scintilla.guide` | **AmigaGuide** (`@Node`, Links) | [VON-MORPHOS-SDK-SCINTILLA.md](VON-MORPHOS-SDK-SCINTILLA.md) |
| `Scintilla/*.h` | C-Header | Lexer/API für Phase 11 |

AmigaGuide auf MorphOS mit AmigaGuide/MultiView lesen; in WSL als durchsuchbarer Text (`@Node MorphOS`, `SCI_SETFONTQUALITY`, …).

Weitere optionale Kopien:

| Priorität | MorphOS-Pfad | Ziel (Vorschlag) |
|-----------|--------------|------------------|
| 2 | `Guide/includes/mui/Scintilla_mcc.h` | `VonMorphosSDK/` — diff zu `AmigaSDK-gcc/.../Scintilla_mcc.h` |
| 3 | `Frameworks/include/mui/MCCScintilla.h` | `VonMorphosSDK/` |

---

## 7. Build, Version, Paketieren (WSL)

```bash
cd ~/development/morphos/AmigaGPT
export PATH="$HOME/development/morphos/flexcat/src/bin_unix:$PATH"
make -f Makefile.MorphOS              # bump BUILD_NUMBER; Anzeige 2.18.<build> — siehe [MORPHOS-VERSION.md](MORPHOS-VERSION.md)
./package-morphos-cross.sh            # DEPLOY=1 → Z:\morphos\out-crosscompile\ + DEPLOY-HISTORY.txt
```

- `make` **ohne** explizites `all` führt **nicht** zwingend `version-bump` aus — Versionsnummer kann vom Binary abweichen, wenn nur teilweise gelinkt wurde.
- Paketieren im Projektsinn: erfolgreicher Deploy nach `Z:\morphos\out-crosscompile\` — siehe [HANDLUNGSANWEISUNG-GIT.md](HANDLUNGSANWEISUNG-GIT.md).
- **AmigaOS3/OS4** (`Makefile`): `BUILD_NUMBER` wird pro `.c`-Compile erhöht (abweichend von MorphOS) — nur dokumentiert, Makefile unverändert.

---

## 8. Roadmap (nächste Schritte)

| Phase | Inhalt | Abhängigkeit |
|-------|--------|--------------|
| **R1–R2** | Stream-Ende, UI-Sync, WANT_READ weich | parallel zu 7c/8 — [STREAM-RECOVERY.md](STREAM-RECOVERY.md) |
| **7c** | Mausrad-Scroll im Chat (NFloattext) | ✓ erledigt |
| **8–10** | String-Safety, optionale Logs, formale DoD | 7a/7b ✓ |
| **11** | Syntax-Highlighting (`SCI_SETLEXER`), Komfort — **`Scintilla.guide` von MorphOS hilfreich** | 8–10 stabil |
| **12** | Chat-Ausgabe: NFloattext → Scintilla + TTEngine | Scintilla im Projekt bewährt |
| **13** | Stream-Worker / UI-Batching | nach Phase 12 |

Details: [SCINTILLA-ARCHITECTURE.md](SCINTILLA-ARCHITECTURE.md).

---

## 9. Fehlersymptome (Scintilla-Integration, Archiv)

| Symptom | Ursache | Fix |
|---------|---------|-----|
| Weißes leeres Fenster | Falsche `MUIM_*`-ID oder falsche Aufrufart | `+2`, `DoMethodA`, `MUIP_Scintilla_Command` |
| „ok“ im Viewer + Freeze | Debug-Probe `SCI_SETTEXT` `"ok"`; `SciFnDirect`/Re-Probe | Probe entfernen, nur Command-Weg |
| Text OK, Version verwirrend | Teil-Build ohne vollständiges `make` | Volles `make -f Makefile.MorphOS` + Paket |

---

## 10. Git / Branch

- Arbeit auf Branch **`scintilla`**, nicht `master` — [HANDLUNGSANWEISUNG-GIT.md](HANDLUNGSANWEISUNG-GIT.md).
- Scintilla-Phase-6-Änderungen: Commit nur auf ausdrückliche Anfrage.

---

## 11. Siehe auch

- [VON-MORPHOS-SDK-SCINTILLA.md](VON-MORPHOS-SDK-SCINTILLA.md) — `VonMorphosSDK/Scintilla.guide` (AmigaGuide), Knoten, TTEngine, Lexer
- [MORPHOS-SDK-ERGAENZUNGEN.md](MORPHOS-SDK-ERGAENZUNGEN.md) — WSL: BIGFOOT vs. AmigaSDK-gcc
- [SCINTILLA-ARCHITECTURE.md](SCINTILLA-ARCHITECTURE.md) — Phasenplan 6–13
- [BUILD-MORPHOS-WSL.md](BUILD-MORPHOS-WSL.md) — WSL-Setup
- [UNICODE-MORPHOS-MUI.md](UNICODE-MORPHOS-MUI.md) — UTF-8, NFloattext, Phase 12-Motivation
- Listing-Quelle: `~/development/morphos/devfiles.txt` (MorphOS SDK-Inventar)
