# Git: Fork und Upstream (dieses Repository)

## Branch-Regel (dieser Fork)

**Nicht direkt auf `master` committen.** Feature-Arbeit auf Topic-Branches (z. B. `scintilla`), dann per Pull Request oder Merge nach `master`, wenn es stabil ist.

```bash
git checkout scintilla    # oder neuen Branch von master
# … arbeiten, committen …
git push -u origin scintilla
```

`master` soll `origin/master` bzw. nach Sync mit `upstream` dem stabilen Stand entsprechen.

---

Dieses Repo ist typischerweise ein **Fork** von [sacredbanana/AmigaGPT](https://github.com/sacredbanana/AmigaGPT). Lokal solltest du zwei Remotes haben:

| Remote     | Zweck |
| ---------- | ----- |
| `origin`   | Dein Fork (Push/Pull für deine Arbeit). |
| `upstream` | Das Original-Repository (Änderungen holen, Vergleich). |

## Einmalig prüfen / setzen

```bash
git remote -v
```

Falls `upstream` fehlt:

```bash
git remote add upstream https://github.com/sacredbanana/AmigaGPT.git
```

## Original einarbeiten

```bash
git fetch upstream
git checkout main   # oder der Default-Branch deines Forks
git merge upstream/main
# alternativ: git rebase upstream/main
git push origin main
```

## Beitrag zurück ins Original

Auf GitHub einen **Pull Request** von einem Branch auf **deinem** Fork (`origin`) gegen `sacredbanana/AmigaGPT` öffnen. Direkten Push auf `upstream` hast du in der Regel nicht.

## Weitere Doku

- [SCINTILLA-ARCHITECTURE.md](SCINTILLA-ARCHITECTURE.md) — Plan für Scintilla.mcc, Streaming und UTF-8.
- [BUILD-MORPHOS-WSL.md](BUILD-MORPHOS-WSL.md) — MorphOS Cross-Build unter WSL2 (Debian) und BIGFOOT SDK.
