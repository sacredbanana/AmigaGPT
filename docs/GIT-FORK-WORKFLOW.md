# Git: Fork und Upstream (dieses Repository)

## Branch-Regel (dieser Fork)

**Nicht direkt auf `master` committen.**

Vollständige Handlungsanweisung (Checklisten, Agenten, Fehlerfälle): **[HANDLUNGSANWEISUNG-GIT.md](HANDLUNGSANWEISUNG-GIT.md)**  
Für Cursor zusätzlich: `.cursor/rules/git-branch-policy.mdc` und [AGENTS.md](../AGENTS.md).

Kurz:

```bash
git checkout master
git pull origin master
git checkout -b feature/kurzbeschreibung
# … arbeiten, committen …
git push -u origin feature/kurzbeschreibung
# Merge nach master (lokal oder PR), Branch löschen
```

`master` = stabiler Fork-Stand (**2.18** seit 2026-05). Der Branch **`scintilla`** war die Entwicklungslinie bis zum Merge — nicht mehr Standard für neue Arbeit.

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

Übersicht: [README.md](README.md)

- [HANDLUNGSANWEISUNG-GIT.md](HANDLUNGSANWEISUNG-GIT.md) — verbindliche Git-Arbeitsweise (Menschen & Agenten).
- [SCINTILLA-ARCHITECTURE.md](SCINTILLA-ARCHITECTURE.md) — Plan für Scintilla.mcc, Streaming und UTF-8.
- [BUILD-MORPHOS-WSL.md](BUILD-MORPHOS-WSL.md) — MorphOS Cross-Build unter WSL2 (Debian) und BIGFOOT SDK.
- [WSL-SETUP-STATUS.md](WSL-SETUP-STATUS.md) — dokumentierter Ist-Stand der WSL-Einrichtung.
