# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Building

**Do not run `make` directly or invent your own toolchain commands.** This is a cross-compiled C project for AmigaOS 3 (m68k), AmigaOS 4 (PPC) and MorphOS (PPC). All builds go through the Dockerized scripts at the repo root:

- AmigaOS 3: `./build_os3.sh`
- AmigaOS 4: `./build_os4.sh`
- MorphOS:   `./build_morphos.sh`

Each script invokes the `sacredbanana/amiga-compiler` Docker image and runs `make && make daemon` against the relevant Makefile (`Makefile`, `Makefile.OS4`, `Makefile.MorphOS`). Outputs land in `out/` as `AmigaGPT_<platform>` and `AmigaGPTD_<platform>`.

Optional env vars (all three scripts):
- `CLEAN=1` — clean before building.
- `DEBUG=1` — adds `-DDEBUG -g -Og` instead of `-Ofast`.

`./package.sh` does a clean build for all three platforms, copies executables and compiled catalogs into `bundle/AmigaGPT/`, and produces `out/AmigaGPT.lha` for distribution.

The host-side test binary (`src/test/`) is a separate harness that builds with the macOS/Linux `gcc` plus `json-c` and OpenSSL — `cd src/test && make` produces `out/openai-test`. It exercises the OpenAI client outside the Amiga environment and needs `src/test/openai-key.h` (template provided).

## Architecture

The repo is one C source tree (`src/`) compiled into two binaries from the same objects:

- **AmigaGPT** — full MUI-based GUI client.
- **AmigaGPTD** — headless daemon. Built with `-DDAEMON`; the Makefiles drop the window/menu source files (`MainWindow.c`, `AboutAmigaGPTWindow.c`, `APIKeyRequesterWindow.c`, `ProxySettingsRequesterWindow.c`, `VoiceInstructionsRequesterWindow.c`, `AmigaGPTTextEditor.c`, `menu.c`) for this variant. Both binaries share the rest of the source and the same compiled catalog object.

Platform conditionals (`__AMIGAOS3__`, `__AMIGAOS4__`, `__MORPHOS__`, `DAEMON`) are scattered through `main.c`, `gui.c`, `speech.c`, etc. — when modifying anything in those files, expect to keep all three platforms compiling. Headers like `gui.h` pull in MUI / Intuition / locale protos and define the shared `OBJECT_ID_*` enum used to look up MUI objects from `app`.

Major modules:
- `main.c` — entry point, CLI vs Workbench startup, stack check, init order (video → speech → GUI → ARexx loop).
- `gui.c` / `MainWindow.c` / `*RequesterWindow.c` — MUI window and gadget construction; the daemon build skips most of these.
- `menu.c` — menu construction (GUI build only).
- `openai.c` / `openai.h` — provider abstraction. Built-in providers are OpenAI, Google Gemini, xAI Grok and Anthropic Claude (chat only); custom HTTP profiles (e.g. LM Studio) are also supported. Conversations are linked lists of `ConversationNode`; `lastResponseId` tracks OpenAI Responses API state. Communicates via AmiSSL.
- `speech.c` — speech subsystem dispatcher. Supports `narrator.device` v34/v37 + `translator.library` (OS3), `flite.device` (OS4), OpenAI TTS (any OS, requires AHI), and ElevenLabs TTS.
- `AmigaGPTConfig.c` — persisted settings, profile storage, API key handling.
- `ARexx.c` — ARexx port handler. The GUI listens on port `AMIGAGPT`, the daemon on `AMIGAGPTD`. Command set is documented in `README.md` (SENDMESSAGE, CREATEIMAGE, SPEAKTEXT, NEWCHAT, LIST*).
- `version.h` — `BUILD_NUMBER` is auto-incremented by `sed` in the Makefile on every C compile; do not hand-edit.

## Localization (gettext catalogs)

Strings are referenced as `STRING_*` symbols generated from `catalogs/AmigaGPT.pot` into `src/AmigaGPT_cat.{c,h}` by `flexcat` during the build. Per-language `.po` files live under `catalogs/<language>/`.

- `make catalog_definition` regenerates `AmigaGPT.pot` from `src/*.c` via `xgettext`.
- `make catalog` runs `flexcat` to regenerate `AmigaGPT_cat.{c,h}`, `msgmerge -U`s every `.po` against the template, and compiles each `<language>/AmigaGPT.catalog`.
- The full build target (`all`) depends on `catalog`, so adding or changing a `STRING_*` will fail at link time until the `.pot` and `_cat.{c,h}` are regenerated.
- To translate after adding new strings, run `/update-translations` (see `.claude/commands/update-translations.md`); it delegates to the `po-translation-updater` agent which validates by running `./build_os3.sh`.

## Repo-specific guidance

- Provider docs: prefer the project MCP servers `openaiDeveloperDocs` (OpenAI) and `xaiDocs` (xAI), and the `openai-docs` / `xai-docs` skills, over general web search.
- AmigaOS / NDK / autodoc / SDK questions, library API lookups, and classic Amiga book recommendations: use the `amiga-sdk-docs` skill. It searches the local `sacredbanana/AmigaSDK-gcc` clone at `.claude/skills/amiga-sdk-docs/cache/amigasdk-gcc` (sync via `./.claude/skills/amiga-sdk-docs/scripts/sync-repo.sh`) and the retro-commodore.eu book catalog. Wait for the sync to finish before falling back to the web. Do not commit anything under `.claude/skills/amiga-sdk-docs/cache/`.
- Building AmigaGPT itself: use the `build-amigagpt` skill — only the three `build_*.sh` scripts work in this environment.
- Before calling an MCP tool, read the installed tool descriptor and follow its schema exactly.
- **ISO 8859-1 Amiga document files** — `bundle/AmigaGPT/AmigaGPT.guide`, `bundle/AmigaGPT/AmigaGPT.readme`, and `bundle/AmigaGPT/Install-AmigaGPT` are read on real Amiga hardware and MUST be stored as ISO 8859-1 (Latin-1), never UTF-8. Before and after any edit to these files: verify with `file -i <path>` that the charset is `iso-8859-1` (or `us-ascii`, which is a safe subset), and scan for multi-byte UTF-8 sequences with `grep -Pn '[^\x00-\xFF]' <path>` (should return nothing). If you find UTF-8 characters (e.g. curly quotes `\xe2\x80\x9c`, em-dashes `\xe2\x80\x94`), replace them with their ISO 8859-1 equivalents or plain ASCII before saving.
