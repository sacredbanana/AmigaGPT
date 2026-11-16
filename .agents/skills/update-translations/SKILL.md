---
name: update-translations
description: Complete all AmigaGPT localisation work. Use this whenever a user-facing string is added, changed or removed (a new or edited STRING_* symbol, an English msgid in catalogs/AmigaGPT.pot, any UI label, menu item, requester text or error message), when catalogs/AmigaGPT.pot changes, when the user runs /update-translations, or when asked to sync or translate the catalog .po files. Do not consider a change that touches user-facing text finished until this has run and the build passes.
---

# Update translations (AmigaGPT gettext catalogs)

Every user-facing string in AmigaGPT is localised into every language under `catalogs/`. A change that adds or edits UI text is **not done** until the template, every `.po` file and the build are all in sync. Run this workflow yourself without being asked.

## 1. Get the template right first

User-facing text in `src/*.c` must be a `STRING_*` symbol, never a literal. For each string you touched:

- **New string**: add an entry to `catalogs/AmigaGPT.pot`. Copy the format of the entries around it:
  ```
  #: src/MainWindow.c
  msgctxt "STRING_SOME_NAME (417//)"
  msgid "English text"
  msgstr "English text"
  ```
  The number in `msgctxt` is the catalog ID. Use the next unused one (check the highest existing ID). Never renumber or reuse an existing ID, because compiled catalogs rely on it.
- **Changed English text**: edit both `msgid` and `msgstr` of the existing entry in the `.pot`. Keep its `msgctxt` and ID.
- **Removed string**: only delete the `.pot` entry once nothing in `src/` references the symbol.

Encoding: the `.pot` and `.po` files are UTF-8 (`flexcat` converts them at build time). The `src/*.c` files are ISO 8859-1, so follow `AMIGA-LATIN1-FILES.md` when editing them.

### BestTextEditor.mcp has its own template

The MUI preferences page of the text editor class (`mcc/BestTextEditor/mcp/`) is a separate library with its own catalog, `BestTextEditor_mcp.catalog`. Its strings are `MCP_*` symbols in `mcc/BestTextEditor/catalogs/BestTextEditor_mcp.pot`, translated in `mcc/BestTextEditor/catalogs/<language>/BestTextEditor_mcp.po` (same language directory names as `catalogs/`). Add, change and remove its entries exactly as above, but in that template, and translate them into every one of those `.po` files. `make mcc` (run by the build scripts) regenerates `mcp/BestTextEditor_mcp_cat.{c,h}` and compiles the catalogs.

## 2. Sync and translate every `.po` file

Delegate to the `po-translation-updater` subagent when your host has it. It is defined for each tool:

- Claude Code: `.claude/agents/po-translation-updater.md`
- Codex: `.codex/agents/po-translation-updater.toml`
- Cursor: `.cursor/agents/po-translation-updater.md`

Prefer running it in the background unless the user wants to watch. **If your host cannot spawn subagents, do the work yourself by following the workflow and quality rules in `.claude/agents/po-translation-updater.md` in full.** Either way, the requirements are:

1. Sync every `catalogs/*/*.po` file from `catalogs/AmigaGPT.pot` (the files are named per language, e.g. `catalogs/german/deutsch.po`). The build runs `msgmerge -U` on each one; a changed `msgid` comes back marked `fuzzy`.
2. Translate only new, empty or fuzzy entries, and remove the `fuzzy` flag once an entry is translated. Preserve existing human translations, placeholders, `msgctxt`, formatting and wrapping.
3. Keep product and technical names (`AmigaGPT`, `OpenAI`, library and device names, version numbers) untranslated unless that file already translates them.

## 3. Validate with the build

Run `./build_os3.sh` (see the `build-amigagpt` skill; never run `make` directly). It regenerates `src/AmigaGPT_cat.{c,h}` with `flexcat` and compiles each `AmigaGPT.catalog`.

If the build fails because a `STRING_*` symbol is referenced in code but missing from the `.pot`, inspect the source context, infer the best English string, add the `.pot` entry, translate it into every `.po` file and retry the build.

## 4. Report

Finish with a short summary: languages updated, new or changed catalog entries, build status, and any translations that need a human to check them. Do not report the task done while any `.po` file still has an untranslated or fuzzy entry for a string you touched.
