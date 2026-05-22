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
| Commits auf **Topic-Branches** (`scintilla`, `feature/…`, `fix/…`) | Commits **direkt auf `master`** |
| Merge/Rebase von `master` in Feature-Branches | Push auf `master` ohne Review/Sync |
| PR von Feature-Branch → `master` (wenn stabil) | Force-Push auf `master` |

**`master`** = stabiler Referenzstand (Fork, ggf. nach Sync mit `upstream`).  
**Feature-Branches** = alle laufende Entwicklung (z. B. Scintilla, Docs, Builds).

---

## 2. Vor jeder Arbeit

```bash
git fetch origin
git checkout scintilla          # oder passenden Topic-Branch
git pull --rebase origin scintilla   # falls Branch schon remote existiert
```

Neuer Topic-Branch von aktuellem `master`:

```bash
git fetch origin
git checkout master
git pull origin master
git checkout -b feature/kurzbeschreibung
```

---

## 3. Während der Arbeit

- Kleine, nachvollziehbare Commits (Thema klar im Subject).
- Nur committen, wenn ausdrücklich gewünscht (Team-/Agent-Regel).
- Keine Secrets (API-Keys, `.env`) ins Repo.
- `git config` im Repo nicht ändern.

---

## 4. Nach der Arbeit (Push)

```bash
git push -u origin <branch-name>
```

**Nicht:** `git push origin master` (außer bewusster Maintainer-Merge nach PR).

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
git checkout scintilla
git merge upstream/master
# Konflikte lösen, testen, dann push
```

Details: [GIT-FORK-WORKFLOW.md](GIT-FORK-WORKFLOW.md).

---

## 6. Agenten / Cursor (Kurz)

1. Aktiven Branch prüfen: **nicht `master`** für neue Commits.
2. Fehlender Branch → `scintilla` oder vom User genannten Topic-Branch verwenden.
3. Nach `git reset`/`merge` auf `master`: User informieren, nicht still auf `master` weitercommitten.
4. Push nur auf Anfrage; nie `master` force-pushen.

Technische Regel für Cursor: `.cursor/rules/git-branch-policy.mdc`  
Übersicht für Tools: [AGENTS.md](../AGENTS.md).

---

## 7. Typische Fehler vermeiden

| Situation | Richtig |
| --------- | ------- |
| Doc- oder Code-Änderung fertig | Commit auf `scintilla` (o. ä.), nicht auf `master` |
| `master` lokal „voraus“ nach Versehen | `git checkout master && git reset --hard origin/master`, Arbeit auf Feature-Branch behalten |
| Remote-Branch divergiert | `git pull --rebase` oder mit User klären; `--force-with-lease` nur bewusst |
| Upstream-Update | Erst `fetch upstream`, Merge in Feature-Branch, testen |

---

## 8. MorphOS-Testpaket: Begriff **Paketieren**

Verbindlich in diesem Fork:

| Begriff | Bedeutung |
| ------- | --------- |
| **Paketieren** | Cross-Build **und** erfolgreiche Bereitstellung unter **`Z:\morphos\out-crosscompile\`** (LHA + Staging `package-morphos`), wie `package-morphos-cross.sh` mit Standard **`DEPLOY=1`**. |
| **Kein abgeschlossenes Paketieren** | Nur `out/AmigaGPT-MorphOS-cross.lha` und/oder `out/package-morphos/` **ohne** erfolgreichen Deploy nach **`Z:`**. |

**Fehler:** Läuft das Skript mit **`DEPLOY=1`** und der Deploy schlägt fehl, ist das ein **fehlgeschlagener Paketierungslauf** (Exit-Code 1). Ursache beheben (`Z:` erreichbar, gleiche Windows-Session wie die Laufwerkszuordnung, PowerShell von WSL) und erneut ausführen. **`DEPLOY=0`** bewusst nur, wenn **kein** Z:-Deploy gewünscht ist (dann liegt **kein** abgeschlossenes Paketieren im obigen Sinn vor).

Skript: `package-morphos-cross.sh` · Kurzbeschreibung: [README.md](README.md).

---

## 9. Verwandte Dokumente

- [GIT-FORK-WORKFLOW.md](GIT-FORK-WORKFLOW.md) — Remotes, Sync, PR ins Original
- [SCINTILLA-ARCHITECTURE.md](SCINTILLA-ARCHITECTURE.md) — Architektur Branch `scintilla`
- [BUILD-MORPHOS-WSL.md](BUILD-MORPHOS-WSL.md) — Cross-Build WSL2/Debian
