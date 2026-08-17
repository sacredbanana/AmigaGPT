# Handlungsanweisung: Git in diesem Fork (weiseb78/AmigaGPT)

Verbindliche Arbeitsweise für Menschen und Assistenten (Cursor, CI, Reviews).

---

## 0. Projektordner (WSL)

**Kanonisch:** `~/development/morphos/AmigaGPT` in WSL (Debian).  
**Cursor-Workspace:** `\\wsl$\Debian\home\<user>\development\morphos\AmigaGPT`

**Nicht** mehr: `C:\Users\xbox\cursorWorkspace\AmigaGPT` — siehe [WSL-SETUP-STATUS.md](WSL-SETUP-STATUS.md).

---

## 1. Grundregel

| Erlaubt | Verboten |
| ------- | -------- |
| Commits auf **Topic-Branches** (`feature/…`, `fix/…`, `chore/…`) | Commits **direkt auf `master`** |
| Merge/Rebase von `master` in Feature-Branches | Push auf `master` ohne Review/Sync |
| PR von Feature-Branch → `master` (wenn stabil) | Force-Push auf `master` |

**`master`** = stabiler Referenzstand (Fork **2.18**-Linie seit 2026-05; ggf. nach Sync mit `upstream`).  
Kein Branch **`main`**.  
**Feature-Branches** = laufende Entwicklung — kurzlebig, nach Merge löschen (z. B. `feature/morphos-startup-feedback`).

Der Branch **`scintilla`** war die Entwicklungslinie bis zum Merge nach `master` (8785); nicht mehr Standard für neue Arbeit.

---

## 2. Vor jeder Arbeit

```bash
git fetch origin
git checkout master
git pull origin master
git checkout -b feature/kurzbeschreibung   # oder fix/…, chore/…
```

---

## 3. Während der Arbeit

- Kleine, nachvollziehbare Commits (Thema klar im Subject).
- **MorphOS-Code:** erst **committen**, wenn der Nutzer den **MorphOS-Test** nach Paketierung als **erfolgreich** bestätigt hat (siehe §9). Ausnahme: Nutzer sagt ausdrücklich „committen“ / reine Doku ohne Laufzeit-Test.
- Keine Secrets (API-Keys, `.env`) ins Repo.
- `git config` im Repo nicht ändern.

---

## 4. Nach der Arbeit (Push)

```bash
git push -u origin <branch-name>
```

**Nur wenn der Nutzer ausdrücklich pushen lässt.**  
**Nicht:** `git push origin master` (außer bewusster Maintainer-Merge nach Freigabe).

Auf GitHub: **Pull Request** `feature/…` → `master` (eigener Fork) oder später ggf. PR zum Upstream.

---

## 5. Remotes

| Remote | URL (Beispiel) | Nutzung |
| ------ | -------------- | ------- |
| `origin` | `https://github.com/weiseb78/AmigaGPT.git` | Push/Pull eigener Arbeit |
| `upstream` | `https://github.com/sacredbanana/AmigaGPT.git` | Nur holen/vergleichen |

Upstream einspielen (auf Feature-Branch, nicht blind auf `master` überschreiben):

```bash
git fetch upstream
git checkout -b feature/upstream-sync   # von aktuellem master
git merge upstream/master
# Konflikte lösen, testen, dann merge nach master
```

### Upstream-Merge: Encoding / System-Prompts (MorphOS)

Upstream **3.0** führt in `src/openai.c` `AMIGA_CHARACTER_SET_OUTPUT_INSTRUCTIONS` ein und hängt sie an jedes Chat-Request (Changelog: Modell soll keine Amiga-untauglichen Unicode-Zeichen senden). Das passt zu **OS3/OS4** (NFloattext + codesets), **nicht** zum MorphOS-Chat (Scintilla, UTF-8).

| Regel | Umsetzung |
| ----- | --------- |
| MorphOS | **Kein** Charset-System-Prompt — `#ifdef __MORPHOS__` nur `conversation->system` (ggf. + xAI-Speech-Tags) |
| OS3/OS4 | Upstream-Prompt behalten |
| Bei jedem `upstream`-Merge | Diff auf neue `instructions` / Charset-/ASCII-Prompts in `openai.c` prüfen; MorphOS-UTF-8 nicht wieder einschleifen |

Hintergrund und Pipelines: [UNICODE-MORPHOS-MUI.md](UNICODE-MORPHOS-MUI.md) §2b.  
Hätte schon beim **3.0-Merge** gesetzt werden sollen; nach **3.1-Sync** nachgezogen (Prompt war unverändert mitgeschleppt).

Details: [GIT-FORK-WORKFLOW.md](GIT-FORK-WORKFLOW.md).

---

## 6. Agenten / Cursor (Kurz)

1. Aktiven Branch prüfen: **nicht `master`** für neue Commits.
2. Neuen Topic-Branch von **`master`**: `feature/…`, `fix/…`, `chore/…` (vom User oder passend benannt).
3. MorphOS-Änderungen: **bauen → paketieren (Z:)** → bei Z:-Fehler **STOP** (Nutzer mountet `Z:`) → Nutzer testet → **dann** Commit auf Topic-Branch (§9).
4. Z:-Deploy fehlgeschlagen → **blockiert** — melden, nicht committen, nicht pushen, kein `DEPLOY=0` als Workaround.
5. Nach `git reset`/`merge` auf `master`: User informieren, nicht still auf `master` weitercommitten.
6. Push nur auf Anfrage; nie `master` force-pushen.

Technische Regel für Cursor: `.cursor/rules/git-branch-policy.mdc`  
Übersicht für Tools: [AGENTS.md](../AGENTS.md).

---

## 7. Typische Fehler vermeiden

| Situation | Richtig |
| --------- | ------- |
| Doc- oder Code-Änderung fertig | Commit auf Topic-Branch, Merge nach `master` — nicht direkt auf `master` committen |
| `master` lokal „voraus“ nach Versehen | `git checkout master && git reset --hard origin/master`, Arbeit auf Feature-Branch behalten |
| Remote-Branch divergiert | `git pull --rebase` oder mit User klären; `--force-with-lease` nur bewusst |
| Upstream-Update | Erst `fetch upstream`, Merge in Feature-Branch; Encoding-Check (§5); testen |

---

## 8. MorphOS-Testpaket: Begriff **Paketieren**

Verbindlich in diesem Fork:

| Begriff | Bedeutung |
| ------- | --------- |
| **Paketieren** | Cross-Build **und** erfolgreiche Bereitstellung von **`AmigaGPT-MorphOS-cross.lha`** unter **`Z:\morphos\out-crosscompile\`** (`package-morphos-cross.sh`, Standard **`DEPLOY=1`**). Entpacken/Update auf MorphOS: **eigenes Deploy-Skript auf der Maschine** (nicht im WSL-Repo). |
| **Kein abgeschlossenes Paketieren** | Nur `out/AmigaGPT-MorphOS-cross.lha` **ohne** erfolgreichen LHA-Deploy nach **`Z:`**. Staging `out/package-morphos/` bleibt lokal in WSL. |

**Fehler:** Läuft das Skript mit **`DEPLOY=1`** und der Deploy schlägt fehl, ist das ein **fehlgeschlagener Paketierungslauf** (Exit-Code 1). Ursache beheben (`Z:` erreichbar, gleiche Windows-Session wie die Laufwerkszuordnung, PowerShell von WSL) und erneut ausführen. **`DEPLOY=0`** bewusst nur, wenn **kein** Z:-Deploy gewünscht ist (dann liegt **kein** abgeschlossenes Paketieren im obigen Sinn vor).

Nach erfolgreichem Z:-Deploy schreibt das Skript **`DEPLOY-HISTORY.txt`** im selben Ordner (letzte 10 Einträge: Version, Git, Build-, Kopierzeit, LHA-MD5 für Abgleich mit Ambient). Details: [MORPHOS-VERSION.md](MORPHOS-VERSION.md).

Skript: `package-morphos-cross.sh` · Kurzbeschreibung: [README.md](README.md).

**Optional (TODO, noch nicht umgesetzt):** Deploy aus reinem WSL per **`smbclient`** auf `//hdsfgo4/share/…` (Credentials per Env), damit kein `Z:` / keine PowerShell-Session nötig ist. Checkliste und Anforderungen: [WSL-SETUP-STATUS.md](WSL-SETUP-STATUS.md) („Deploy per smbclient“). Bis dahin: `DEPLOY=0` + manuell nach `Z:\…`, oder persistentes `net use` in Windows.

**Agent/Cursor (Standard):** Nach jedem Cross-Build für MorphOS-Tests **immer paketieren** (`./package-morphos-cross.sh`), solange der Nutzer nichts anderes sagt (z. B. „nur bauen“, `DEPLOY=0`). Regel: `.cursor/rules/morphos-build-package.mdc`.

---

## 9. MorphOS-Entwicklungszyklus (verbindlich)

| Schritt | Wer | Was |
| ------- | --- | --- |
| 1 | Agent | Änderung umsetzen |
| 2 | Agent | `make -f Makefile.MorphOS` |
| 3 | Agent | `./package-morphos-cross.sh` (Standard `DEPLOY=1`) |
| 4 | Agent | Version, MD5, Z:-Pfad melden; bei **Deploy-Fehler** **STOP** — Nutzer: **`Z:` mounten**; **kein Commit**, **kein Push**, nicht „fertig“ |
| 5 | Nutzer | Auf MorphOS testen (LHA von Z:, eigenes Deploy-Skript), Rückmeldung |
| 6 | Agent | **Commit** erst nach **erfolgreicher** Rückmeldung — auf **Topic-Branch**, nicht `master` |

**Paketieren** im Sinne von §8 ist nur bei **erfolgreichem** Z:-Deploy abgeschlossen. Lokal nur `out/…lha` ohne Z: reicht dem Nutzer für den Testloop nicht als „fertig paketiert“.

**Nicht verhandelbar:** „Fertig machen“, Merge abschließen oder große Aufgaben erlauben **keinen** Schritt 6 ohne Schritt 4 (Z: OK) und Schritt 5 (Nutzer OK). **`DEPLOY=0` nach Z:-Fehler** ist verboten.

Push weiterhin nur auf Anfrage. Neue Arbeit: Topic-Branch von `master`, nicht direkt auf `master` committen.

---

## 10. Verwandte Dokumente

- [HANDLUNGSANWEISUNG-MORPHOS-AGENT.md](HANDLUNGSANWEISUNG-MORPHOS-AGENT.md) — **Agent:** Restart, ASL/PushMethod, Scintilla-Chat, Regressionen
- [GIT-FORK-WORKFLOW.md](GIT-FORK-WORKFLOW.md) — Remotes, Sync, PR ins Original
- [MORPHOS-RELEASE-NOTES.md](MORPHOS-RELEASE-NOTES.md) — Nutzer-Release-Notes Fork (z. B. 2.18); `CHANGELOG.md` = upstream
- [SCINTILLA-ARCHITECTURE.md](SCINTILLA-ARCHITECTURE.md) — Architektur Scintilla / Streaming (historisch Branch `scintilla`, jetzt in `master`)
- [BUILD-MORPHOS-WSL.md](BUILD-MORPHOS-WSL.md) — Cross-Build WSL2/Debian
