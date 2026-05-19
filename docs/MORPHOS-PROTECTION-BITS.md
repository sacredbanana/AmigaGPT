# MorphOS: Protection Bits beim Cross-Build

## Kurzantwort

**Ja, das kann ein Problem sein** — aber nur für **Datei-Attribute**, nicht für den Programmcode selbst.

Der Cross-Compiler erzeugt gültige **MorphOS-Executables** (HUNK/ELF). Was fehlt, sind die **AmigaDOS-Protection-Bits** auf dem Dateisystem (`e` = ausführbar, `s` = Script, …). Die werden von Linux/WSL **nicht** gesetzt und von **`lha`/`zip` unter Linux nicht** ins Archiv geschrieben (Listing zeigt `[generic]`, nicht Amiga-Attribute).

---

## Was betroffen ist

| Dateityp | Benötigte Bits | Symptom ohne Fix |
|----------|----------------|------------------|
| `AmigaGPT_MorphOS`, `AmigaGPTD_MorphOS` | **+e** (execute) | Doppelklick/Run von Workbench schlägt fehl |
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

Im Ordner `AmigaGPT/AmigaGPT/`:

```text
rx fix-protection.rexx
```

oder manuell:

```text
protect AmigaGPT_MorphOS +e
protect AmigaGPTD_MorphOS +e
protect rexx/#? +s
```

Das Skript `fix-protection.rexx` liegt im Paket von `package-morphos-cross.sh`.

**Shell-Start** (`run AMIGAGPT:AmigaGPT_MorphOS`) funktioniert manchmal auch ohne `+e`; **Workbench** braucht die Bits fast immer.

---

## Langfristig

| Weg | Protection Bits |
|-----|-----------------|
| Offizielles Release + Installer | korrekt |
| LHA auf **MorphOS** neu packen (nach `protect`) | korrekt im Archiv |
| Cross-Paket + `fix-protection.rexx` | einmaliger manueller Schritt |
| Nur SMB-Kopie ohne Archiv | am unzuverlässigsten |

Ein Linux-Tool, das echte Amiga-LHA-Attribute schreibt, ist in der Standard-Debian-Toolchain praktisch nicht verfügbar — deshalb der Fix auf der MorphOS-Seite.
