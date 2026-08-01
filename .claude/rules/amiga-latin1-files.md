---
paths:
  - "bundle/AmigaGPT/Install-AmigaGPT"
  - "bundle/AmigaGPT/AmigaGPT.readme"
  - "bundle/AmigaGPT/AmigaGPT.guide"
  - "bundle/**/*.rexx"
  - "assets/**/*.rexx"
---

# Amiga Latin-1 files (HARD RULE)

Follow **[AMIGA-LATIN1-FILES.md](../../AMIGA-LATIN1-FILES.md)** at the repository root. That file is the canonical rule for all agents.

When editing any path listed above:

1. Read `AMIGA-LATIN1-FILES.md` if you have not already this session.
2. Run its verification script **before and after** your edit.
3. Do **not** commit if verification fails.
4. Do **not** use normal UTF-8 patch tools for non-ASCII Latin-1 text; use `python3` with `read_bytes()` / `write_bytes()`.
5. Warn the user to reload the editor tab after git stash/checkout so a stale buffer does not re-corrupt the file on save.
