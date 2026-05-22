# MorphOS: Protection Bits beim Cross-Build

## Kurzantwort

**Ja, das kann ein Problem sein** — aber nur für **Datei-Attribute**, nicht für den Programmcode selbst.

Der Cross-Compiler erzeugt gültige **MorphOS-Executables** (HUNK/ELF). Was fehlt, sind die **AmigaDOS-Protection-Bits** auf dem Dateisystem (`e` = ausführbar, `s` = Script, …). Die werden von Linux/WSL **nicht** gesetzt und von **`lha`/`zip` unter Linux nicht** ins Archiv geschrieben (Listing zeigt `[generic]`, nicht Amiga-Attribute).

**Praxis (Cross-Test):** Die Binaries laufen oft schon per **Shell** (`run AmigaGPT_MorphOS`) **ohne** `fix-protection`. **+s** für `rexx/*.rexx` ist für die mitgelieferten ARexx-Skripte relevant — bei euch noch **ungetestet**. **+e** hilft vor allem für **Workbench**-Start.

---

## Was betroffen ist

| Dateityp | Benötigte Bits | Symptom ohne Fix |
|----------|----------------|------------------|
| `AmigaGPT_MorphOS`, `AmigaGPTD_MorphOS` | **+e** (execute) | Doppelklick/Run von Workbench schlägt oft fehl |
| `rexx/*.rexx` | **+s** (script) | ARexx-Skripte laufen nicht |
| `*.info` | (keine Protection; eigene Icon-Datei) | Stack/Icon aus `.info` — im Paket enthalten |
| `*.catalog` | meist nur rw | selten kritisch |

**Nicht betroffen:** Inhalt der Binaries, `.info`-Metadaten (sofern mitkopiert), Katalog-Dateien.

---

## Warum das bei uns passiert

1. Build unter **WSL/Linux** → nur Unix-Rechte (`chmod +x` hilft MorphOS nicht).
2. **LHA mit `lha`/`jlha`** aus Debian → Archiv ohne Amiga-Protection-Metadaten.
3. Kopie über **SMB/Z:** → Bits gehen oft ebenfalls verloren.

Der offizielle **Installer** setzt Bits korrekt (`copyfiles` mit `(infos)`, `protect "C:AskGPT" "+s"`). Unser **Cross-Testpaket** umgeht den Installer bewusst.

---

## Fix nach dem Entpacken (MorphOS)

Im Ordner `AmigaGPT/AmigaGPT/` (nach LHA-Entpacken):

```text
rx fix-protection.rexx
```

oder manuell:

```text
protect AmigaGPT_MorphOS +e
protect AmigaGPTD_MorphOS +e
protect rexx/#? +s
```

`fix-protection.rexx` wird von `package-morphos-cross.sh` ins Paket geschrieben (Heredoc im Staging).

---

## ARexx: erste Zeile muss ein Kommentar sein

Laut [AmigaOS Manual: ARexx – Elements of ARexx](https://wiki.amigaos.net/wiki/AmigaOS_Manual:_ARexx_Elements_of_ARexx) (*Comments*):

> **Each ARexx program must begin with a comment.**

Ein gültiger Kopf ist z. B.:

```rexx
/* fix-protection */
Address COMMAND
...
```

Ohne `/* … */` in **Zeile 1** meldet `rx` oft **Fehler 5** — auch wenn der restliche Code korrekt ist. Das gilt für `fix-protection.rexx` und alle Skripte unter `rexx/`.

---

## Cross-Paket bereitstellen (WSL → Share)

`./package-morphos-cross.sh` baut (optional), erzeugt `out/package-morphos/` und `out/AmigaGPT-MorphOS-cross.lha`, kopiert bei **`DEPLOY=1`** nach **`Z:\morphos\out-crosscompile\`** (LHA + Ordner `package-morphos`).

Auf MorphOS z. B.:

```text
HDSFGO4-share:morphos/out-crosscompile/AmigaGPT-MorphOS-cross.lha
```

entpacken, dann optional `rx fix-protection.rexx` im App-Ordner, `assign AMIGAGPT:` setzen, starten.

Experimentelle **Auto-Deploy-ARexx-Skripte** (`deploytoram` usw.) sind aus dem Repo entfernt; nicht mehr Teil des Paketierens.

---

## Langfristig

| Weg | Protection Bits |
|-----|-----------------|
| Offizielles Release + Installer | korrekt |
| LHA auf **MorphOS** neu packen (nach `protect`) | korrekt im Archiv |
| Cross-Paket + `fix-protection.rexx` | einmaliger manueller Schritt (v. a. rexx) |
| Nur SMB-Kopie ohne Archiv | am unzuverlässigsten |

Ein Linux-Tool, das echte Amiga-LHA-Attribute schreibt, ist in der Standard-Debian-Toolchain praktisch nicht verfügbar — deshalb der Fix auf der MorphOS-Seite.
