# Agent instructions (AmigaGPT fork)

This repository is **weiseb78/AmigaGPT** — a fork of [sacredbanana/AmigaGPT](https://github.com/sacredbanana/AmigaGPT).

**Canonical workspace path (WSL only):** `~/development/morphos/AmigaGPT` — not `C:\Users\xbox\cursorWorkspace\AmigaGPT`. Open via `\\wsl$\Debian\home\<user>\development\morphos\AmigaGPT` in Cursor.

**Rules vs. Handlung:** `.cursor/rules/*.mdc` sind die **kurzen, immer (bzw. projektweit) geladenen** Checklisten für das Modell. `docs/HANDLUNGSANWEISUNG-*.md` ist der **vollständige** verbindliche Text — gleicher Ablauf, nicht zwei verschiedene Flows. Wenn etwas kollidiert, gewinnt die Handlung; die Rules sollten dann angepasst werden.

## Git (required)

Read and follow **`docs/HANDLUNGSANWEISUNG-GIT.md`**.

Summary:

- **Do not commit on `master`.** Use topic branches; default: **`scintilla`**.
- Push only when the user asks. Never force-push `master`.
- `origin` = fork; `upstream` = upstream original (fetch/merge into feature branches).

Cursor rule: `.cursor/rules/git-branch-policy.mdc` (always applied).

## Docs map

Start: `docs/README.md`

| Document | Purpose |
| -------- | ------- |
| `docs/HANDLUNGSANWEISUNG-GIT.md` | Binding git workflow (DE) |
| `docs/HANDLUNGSANWEISUNG-MORPHOS-AGENT.md` | MorphOS runtime: restart, ASL, Scintilla (Agent, DE) |
| `docs/GIT-FORK-WORKFLOW.md` | Remotes, upstream sync, PRs |
| `docs/SCINTILLA-ARCHITECTURE.md` | Scintilla / streaming / UTF-8 / Fence-Parser / Chat-Anzeige vs. `codeblocks` |
| `docs/PHASE-10-DOD.md` | DoD Code-Viewer v0.1 (Phasen 6–10, MorphOS) |
| `docs/STREAM-RECOVERY.md` | Stream & chat recovery (R1–R4): WANT_READ, UI sync, freeze |
| `docs/BUILD-MORPHOS-WSL.md` | MorphOS cross-build on WSL2 Debian (incl. FlexCat bootstrap) |
| `docs/MORPHOS-SDK-ERGAENZUNGEN.md` | Exact SDK supplements (BIGFOOT vs AmigaSDK-gcc) |
| `docs/WSL-SETUP-STATUS.md` | WSL environment setup status / checklist (DE) |
| `tools/test-utf8stream.sh` | Host unit tests for `utf8stream` (WSL `gcc`) |

## Build & test loop (MorphOS) — mandatory

**Every `make` for this repo must be followed by packaging in the same agent turn**, unless the user explicitly opted out (nur bauen / ohne Paket / `DEPLOY=0`).

Preferred one-liner:

```bash
./ship-morphos.sh
# or: make -f Makefile.MorphOS ship
```

That runs **build + daemon + `package-morphos-cross.sh` (Z:)**. Do **not** stop after `make` alone.

Report after package: **version** (`2.18.BUILD_NUMBER`), **MD5**, **`Z:/morphos/out-crosscompile/AmigaGPT-MorphOS-cross.lha`**. If Z: deploy fails → say so; do not claim “paketiert”; do not commit.

Then: user tests on MorphOS → commit only after OK (not on `master`).

**Automation:** `.cursor/hooks.json` nudges the agent after bare `make` and on `stop` if packaging was skipped. **Rules:** `.cursor/rules/morphos-build-package.mdc`, workspace `development/.cursor/rules/morphos-make-then-package.mdc`. **Optional global rule:** `docs/CURSOR-USER-RULE-SNIPPET.txt`.

See `docs/HANDLUNGSANWEISUNG-GIT.md` §8–9, `docs/HANDLUNGSANWEISUNG-MORPHOS-AGENT.md`.
