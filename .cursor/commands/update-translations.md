Use the `po-translation-updater` subagent to update all gettext translations for this repository.

Requirements:

1. Sync every `catalogs/*/*.po` file from `catalogs/AmigaGPT.pot`.
2. Translate only new, empty, or fuzzy entries while preserving existing human translations, placeholders, `msgctxt`, formatting, and wrapping.
3. Validate the result by running `./build_os3.sh`.
4. If the build fails because a localized `STRING_*` symbol is referenced in code but missing from `catalogs/AmigaGPT.pot`, inspect the source context, infer the best English string, add the missing `.pot` entry, update all `.po` files, and retry the build.
5. Prefer running the subagent in the background unless the user explicitly asks to watch it live.

Return the subagent's final summary, including updated languages, new catalog entries, build status, and anything needing human review.
