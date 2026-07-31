# Agent instructions (AmigaGPT fork)

This repository is **weiseb78/AmigaGPT** — a fork of [sacredbanana/AmigaGPT](https://github.com/sacredbanana/AmigaGPT).

**Canonical workspace path (WSL only):** `~/development/morphos/AmigaGPT` — not `C:\Users\xbox\cursorWorkspace\AmigaGPT`. Open via `\\wsl$\Debian\home\<user>\development\morphos\AmigaGPT` in Cursor.

**Rules vs. Handlung:** `.cursor/rules/*.mdc` sind die **kurzen, immer (bzw. projektweit) geladenen** Checklisten für das Modell. `docs/HANDLUNGSANWEISUNG-*.md` ist der **vollständige** verbindliche Text — gleicher Ablauf, nicht zwei verschiedene Flows. Wenn etwas kollidiert, gewinnt die Handlung; die Rules sollten dann angepasst werden.

## Provider docs (upstream)

- For OpenAI questions, prefer the project MCP server `openaiDeveloperDocs` and the project skill `openai-docs`.
- For xAI questions, prefer the project MCP server `xaiDocs` and the project skill `xai-docs`.
- For AmigaOS 3, AmigaOS 4.1, MorphOS, SDK, NDK, autodoc, include, or example-code questions, prefer the project skill `amiga-sdk-docs`.
- For `amiga-sdk-docs`, sync and search the local cache at `.cursor/cache/amigasdk-gcc` before broader web search.
- For `amiga-sdk-docs`, wait for the sync script to finish successfully or fail before any fallback to GitHub pages or broader web search.
- Before calling an MCP tool, read the installed tool descriptor and follow its schema exactly.
- For documentation answers, prefer MCP-backed official docs over general web search.
- If MCP is unavailable, fall back only to official provider docs pages:
  - OpenAI: `developers.openai.com`, `platform.openai.com`
  - xAI: `docs.x.ai`

## Git (required)

Read and follow **`docs/HANDLUNGSANWEISUNG-GIT.md`**.

Summary:

- **Do not commit on `master`.** Use topic branches from `master` (`feature/*`, `fix/*`, `chore/*`).
- Push only when the user asks. Never force-push `master`. Prefer **`master`**, not `main`.
- `origin` = fork; `upstream` = upstream original (fetch/merge into feature branches).

Cursor rule: `.cursor/rules/git-branch-policy.mdc` (always applied).

## Docs map (this repo — sufficient for GitHub clone)

Start: `docs/README.md` · Policy: `docs/DOKUMENTATION-WORKSPACE.md`

| Document | Purpose |
| -------- | ------- |
| `docs/HANDLUNGSANWEISUNG-GIT.md` | Binding git workflow (DE) |
| `docs/HANDLUNGSANWEISUNG-MORPHOS-AGENT.md` | MorphOS runtime for AmigaGPT only (complete in-repo) |
| `docs/GIT-FORK-WORKFLOW.md` | Remotes, upstream sync, PRs |
| `docs/SCINTILLA-ARCHITECTURE.md` | Scintilla / streaming / UTF-8 / Fence-Parser / Chat-Anzeige vs. `codeblocks` |
| `docs/PHASE-10-DOD.md` | DoD Code-Viewer v0.1 (Phasen 6–10, MorphOS) |
| `docs/PHASE-12-CHAT-SCINTILLA.md` | Phase 12 + 12.1 Chat-Scintilla, Midi-Markdown, Testplan (MorphOS) |
| `docs/CHAT-FIND-SCINTILLA.md` | Chat-Suche + User-Sprünge (MorphOS) |
| `docs/MIDI-MARKDOWN-ROADMAP.md` | Export, Midi-Markdown, Hotspots, Tabellen, Zeilenumbruch |
| `docs/STREAM-RECOVERY.md` | Stream & chat recovery (R1–R4): WANT_READ, UI sync, freeze |
| `docs/BUILD-MORPHOS-WSL.md` | MorphOS cross-build on WSL2 Debian (incl. FlexCat bootstrap) |
| `docs/MORPHOS-SDK-ERGAENZUNGEN.md` | Exact SDK supplements (BIGFOOT vs AmigaSDK-gcc) |
| `docs/MORPHOS-RELEASE-NOTES.md` | Fork release notes (e.g. 2.18); `CHANGELOG.md` = upstream |
| `docs/WSL-SETUP-STATUS.md` | WSL environment setup status / checklist (DE) |
| `tools/test-utf8stream.sh` | Host tests: `utf8stream` (WSL `gcc`) |
| `tools/test-chatmd-markers.sh` | Host tests: Kursiv/Fett-`*` Heuristik (`chatmd_markers.c`) |
| `tools/test-codefence.sh` | Host tests: Fence-Parser 3+ Backticks (`codefence.c`) |

## Build & test loop (MorphOS) — mandatory

**Every `make` for this repo must be followed by packaging in the same agent turn**, unless the user explicitly opted out (nur bauen / ohne Paket / `DEPLOY=0`).

Preferred one-liner:

```bash
bash ship-morphos.sh
# or: make -f Makefile.MorphOS ship
```

That runs **build + daemon + `package-morphos-cross.sh` (Z:)**. Do **not** stop after `make` alone.

Report after package: **version** (`2.18.BUILD_NUMBER`), **MD5**, **`Z:/morphos/out-crosscompile/AmigaGPT-MorphOS-cross.lha`**. If Z: deploy fails → **STOP** — tell user to mount **`Z:`**; do not claim “paketiert”; do not commit or push; do not use `DEPLOY=0` to continue.

Then: user tests on MorphOS → commit only after explicit **MorphOS OK** on a topic branch (not `master`).

**Automation:** `.cursor/hooks.json` nudges the agent after bare `make` and on `stop` if packaging was skipped or failed. **Rules:** `.cursor/rules/morphos-build-package.mdc`, **`.cursor/rules/morphos-hard-gates.mdc`**, workspace `development/.cursor/rules/morphos-make-then-package.mdc`.

See `docs/HANDLUNGSANWEISUNG-GIT.md` §8–9, `docs/HANDLUNGSANWEISUNG-MORPHOS-AGENT.md`.
