# Dokumentation: GitHub-Repo vs. Workspace

## Drei Kanäle

| Kanal | Ort | Remote | Wer liest es? |
| ----- | --- | ------ | ------------- |
| **Repo (GitHub)** | `AmigaGPT/docs/`, `AGENTS.md` | `github.com` (Fork) | Jeder Klon von `weiseb78/AmigaGPT` — muss **ohne** Nachbarordner funktionieren |
| **Workspace-Docs (Gitea)** | `~/development/morphos/docs/` | Gitea `morphos-docs` | Versionierung über Rechner (WSL, MorphOS, …); **nicht** Ersatz für `AmigaGPT/docs/` |
| **Nachbar (Gitea)** | `~/development/morphos/MarkdownEdit/` | Gitea (Produkt) | Eigenes Repo; AmigaGPT-Agent **nicht** patchen |

**Gitea `morphos-docs` (Klartext, kein relativer Link im Repo):**

```text
ssh://xbox@192.168.50.96:2222/giteaadmin/morphos-docs.git
```

Clone neben AmigaGPT:

```bash
mkdir -p ~/development/morphos
cd ~/development/morphos
git clone ssh://xbox@192.168.50.96:2222/giteaadmin/morphos-docs.git docs
```

Updates: `cd ~/development/morphos/docs && git pull`

## Regeln (verbindlich für Commits in AmigaGPT)

1. **Keine Markdown-Links** aus diesem Repo nach oben (`../docs/`, `../../docs/`) — auf GitHub sind das **tote Links**.
2. **Nicht** „nur verlinken“ für Inhalte, die ein Agent im Repo braucht: Wesentliches steht in **`docs/HANDLUNGSANWEISUNG-MORPHOS-AGENT.md`** (und den übrigen Projekt-Docs) **vollständig**.
3. **Workspace-Docs** sind die **gemeinsame Wissensbasis** über Projekte hinweg (MarkdownEdit, AmigaGPT, …). Neue übergreifende Learnings dort pflegen — **und** ins Projekt-Repo übernehmen, wenn AmigaGPT davon abhängt (Kurzfassung oder Verweis als Klartext-Pfad, kein relativer Link).
4. **Cursor** (`development/.cursor/rules/morphos-handlungsanweisung.mdc`) darf auf `morphos/docs/` zeigen — liegt außerhalb des GitHub-Repos.

## Workspace-Pfade (nur lokal, nicht als Link-Ziel im Repo)

```text
~/development/morphos/docs/                                   # Git: morphos-docs (Gitea)
~/development/morphos/docs/HANDLUNGSANWEISUNG-MORPHOS.md      # Build, Z:, HW, TaskList
~/development/morphos/docs/HANDLUNGSANWEISUNG-MORPHOS-MUI.md  # MUI, Quit, ASL
~/development/morphos/MarkdownEdit/                           # eigenes Gitea-Produktrepo
```

## Sync-Richtung

```text
morphos/docs  ──(manuell, bei übergreifenden Learnings)──►  AmigaGPT/docs
         ▲
         └── nicht: AmigaGPT verlinkt nur nach oben und bleibt leer
```

**Beispiel:** Relaunch-Lock → Detail in `MORPHOS-STABILITAET.md` (Repo). Allgemeine „make → package in einer Antwort“-Regel → in `morphos/docs` **und** bereits in `HANDLUNGSANWEISUNG-GIT.md` / Agent-Handlung im Repo.
