# Agent instructions (AmigaGPT fork)

This repository is **weiseb78/AmigaGPT** — a fork of [sacredbanana/AmigaGPT](https://github.com/sacredbanana/AmigaGPT).

**Canonical workspace path (WSL only):** `~/development/morphos/AmigaGPT` — not `C:\Users\xbox\cursorWorkspace\AmigaGPT`. Open via `\\wsl$\Debian\home\<user>\development\morphos\AmigaGPT` in Cursor.

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
| `docs/GIT-FORK-WORKFLOW.md` | Remotes, upstream sync, PRs |
| `docs/SCINTILLA-ARCHITECTURE.md` | Scintilla / streaming / UTF-8 / Fence-Parser / Chat-Anzeige vs. `codeblocks` |
| `docs/STREAM-RECOVERY.md` | Stream & chat recovery (R1–R4): WANT_READ, UI sync, freeze |
| `docs/BUILD-MORPHOS-WSL.md` | MorphOS cross-build on WSL2 Debian (incl. FlexCat bootstrap) |
| `docs/MORPHOS-SDK-ERGAENZUNGEN.md` | Exact SDK supplements (BIGFOOT vs AmigaSDK-gcc) |
| `docs/WSL-SETUP-STATUS.md` | WSL environment setup status / checklist (DE) |
| `tools/test-utf8stream.sh` | Host unit tests for `utf8stream` (WSL `gcc`) |

## Build (MorphOS)

`make -f Makefile.MorphOS` — see `docs/BUILD-MORPHOS-WSL.md`.
