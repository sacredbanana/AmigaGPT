# Dokumentation: GitHub-Repo vs. Workspace

## Zwei Kanäle

| Kanal | Ort | Wer liest es? |
| ----- | --- | ------------- |
| **Repo (GitHub)** | `AmigaGPT/docs/`, `AGENTS.md` | Jeder Klon von `weiseb78/AmigaGPT` — muss **ohne** Nachbarordner funktionieren |
| **Workspace** | `~/development/morphos/docs/` | Nur bei Entwicklung unter `~/development/morphos/` (Cursor-Regeln, du) |

## Regeln (verbindlich für Commits in AmigaGPT)

1. **Keine Markdown-Links** aus diesem Repo nach oben (`../docs/`, `../../docs/`) — auf GitHub sind das **tote Links**.
2. **Nicht** „nur verlinken“ für Inhalte, die ein Agent im Repo braucht: Wesentliches steht in **`docs/HANDLUNGSANWEISUNG-MORPHOS-AGENT.md`** (und den übrigen Projekt-Docs) **vollständig**.
3. **Workspace-Docs** sind die **gemeinsame Wissensbasis** über Projekte hinweg (MarkdownEdit, AmigaGPT, …). Neue übergreifende Learnings dort pflegen — **und** ins Projekt-Repo übernehmen, wenn AmigaGPT davon abhängt (Kurzfassung oder Verweis als Klartext-Pfad, kein relativer Link).
4. **Cursor** (`development/.cursor/rules/morphos-handlungsanweisung.mdc`) darf auf `morphos/docs/` zeigen — liegt außerhalb des GitHub-Repos.

## Workspace-Pfade (nur lokal, nicht als Link-Ziel im Repo)

```text
~/development/morphos/docs/HANDLUNGSANWEISUNG-MORPHOS.md      # Build, Z:, HW, TaskList
~/development/morphos/docs/HANDLUNGSANWEISUNG-MORPHOS-MUI.md  # MUI, Quit, ASL
```

## Sync-Richtung

```text
morphos/docs  ──(manuell, bei übergreifenden Learnings)──►  AmigaGPT/docs
         ▲
         └── nicht: AmigaGPT verlinkt nur nach oben und bleibt leer
```

**Beispiel:** Relaunch-Lock → Detail in `MORPHOS-STABILITAET.md` (Repo). Allgemeine „make → package in einer Antwort“-Regel → in `morphos/docs` **und** bereits in `HANDLUNGSANWEISUNG-GIT.md` / Agent-Handlung im Repo.
