# Git: Fork und Upstream (dieses Repository)

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
