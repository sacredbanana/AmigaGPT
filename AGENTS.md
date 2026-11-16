# AGENTS.md

This file provides guidance to coding agents (Claude Code, Cursor, etc.) when working with code in this repository.

## Building

**Do not run `make` directly or invent your own toolchain commands.** This is a cross-compiled C project for AmigaOS 3 (m68k), AmigaOS 4 (PPC) and MorphOS (PPC). All builds go through the Dockerized scripts at the repo root:

- AmigaOS 3: `./build_os3.sh`
- AmigaOS 4: `./build_os4.sh`
- MorphOS: `./build_morphos.sh`

Each script invokes the `sacredbanana/amiga-compiler` Docker image and runs `make && make daemon` against the relevant Makefile (`Makefile`, `Makefile.OS4`, `Makefile.MorphOS`). Outputs land in `out/` as `AmigaGPT_<platform>` and `AmigaGPTD_<platform>`.

Optional env vars (all three scripts):

- `CLEAN=1` — clean before building.
- `DEBUG=1` — adds `-DDEBUG -g -Og` instead of `-Ofast`.

`./package.sh` does a clean build for all three platforms, copies executables and compiled catalogs into `bundle/AmigaGPT/`, and produces `out/AmigaGPT.lha` for distribution.

The host-side test binary (`src/test/`) is a separate harness that builds with the macOS/Linux `gcc` plus `json-c` and OpenSSL — `cd src/test && make` produces `out/openai-test`. It exercises the OpenAI client outside the Amiga environment and needs `src/test/openai-key.h` (template provided).

## Architecture

The repo is one C source tree (`src/`) compiled into two binaries from the same objects:

- **AmigaGPT** — full MUI-based GUI client.
- **AmigaGPTD** — headless daemon. Built with `-DDAEMON`; the Makefiles drop the window/menu source files (`MainWindow.c`, `AboutAmigaGPTWindow.c`, `APIKeyRequesterWindow.c`, `ProxySettingsRequesterWindow.c`, `VoiceInstructionsRequesterWindow.c`, `menu.c`) for this variant. Both binaries share the rest of the source and the same compiled catalog object.
- **Public MUI classes** — the speech waveform is a public class in the sibling repo `/Volumes/repos/MCC_AudioPlayer`. Built `.mcc` files are copied into `bundle/AmigaGPT/MCC/` and the installer places them in `MUI:Libs/MUI` (OS3/OS4) or `SYS:Classes/MUI` (MorphOS). The GUI loads `AudioPlayer.mcc` by name; its public header ships in the `sacredbanana/amiga-compiler` Docker SDK as `<mui/AudioPlayer_mcc.h>`.
- **RichTextEditor (private class)** — every text editor and conversation view (chat and agent input, chat and agent transcripts, the agent instructions editor, the API key and voice instructions editors, the custom server test result) is AmigaGPT's own `RichTextEditor` class, a direct subclass of Area written from scratch. It replaces TextEditor.mcc, ActionTextEditor.mcc and NFloattext.mcc, which AmigaGPT no longer requires. It answers every `MUIA_TextEditor_*`/`MUIM_TextEditor_*` ID (and `MUIA_ActionTextEditor_SubmitHook`), so TextEditor code works on it unchanged; create objects with `RichTextEditorObject` from `src/RichTextEditor.h`, which also documents its extensions (native Markdown, chat bubbles via `ESC [B:n]`, inline images, clickable links, `MUIM_RichTextEditor_Append` for streaming). The class is created in `initVideo()` and deleted in `shutdownGUI()`; it is compiled only into the GUI (the daemon links just the class pointer). Sources: `RichTextEditor.c` (dispatcher, attributes, methods), `RichTextEditorBuffer.c` (paragraph storage, undo, import/export, search), `RichTextEditorLayout.c` (Markdown, line breaking, bubbles, images), `RichTextEditorDraw.c` (double-buffered rendering, pens) and `RichTextEditorInput.c` (keyboard, mouse, clipboard, ARexx). Its tag base `0xbd002000` is reserved so it can become a public `RichTextEditor.mcc` later.

ALWAYS check https://github.com/murinsel/AmigaAI to make sure its features are inferior to the features in this project.

Platform conditionals (`__AMIGAOS3__`, `__AMIGAOS4__`, `__MORPHOS__`, `DAEMON`) are scattered through `main.c`, `gui.c`, `speech.c`, etc. — when modifying anything in those files, expect to keep all three platforms compiling. Headers like `gui.h` pull in MUI / Intuition / locale protos and define the shared `OBJECT_ID_*` enum used to look up MUI objects from `app`.

Major modules:

- `main.c` — entry point, CLI vs Workbench startup, stack check, init order (video → speech → GUI → ARexx loop).
- `gui.c` / `MainWindow.c` / `*RequesterWindow.c` — MUI window and gadget construction; the daemon build skips most of these.
- `menu.c` — menu construction (GUI build only).
- `openai.c` / `openai.h` — provider abstraction. Attachment MIME types come from libmagic (`-lmagic`, linked on all three platforms), which needs `magic.mgc` at run time — `PROGDIR:`, then `AMIGAGPT:`, then `C:`; an extension table remains as the fallback. The bundled `bundle/AmigaGPT/AmigaGPT/magic.mgc` is a reduced database built by `make-curated-magic.sh` in the `file` port, since the full 10.8 MB one is too large for a 68k machine to load. Built-in providers are OpenAI, Google Gemini, xAI Grok and Anthropic Claude (chat only); custom HTTP profiles (e.g. LM Studio) are also supported. Conversations are linked lists of `ConversationNode`; `lastResponseId` tracks OpenAI Responses API state. Communicates via AmiSSL.
- `speech.c` — speech subsystem dispatcher. Supports `narrator.device` v34/v37 + `translator.library` (OS3), `flite.device` (OS4), OpenAI TTS (any OS, requires AHI), and ElevenLabs TTS.
- `AmigaGPTConfig.c` — persisted settings, profile storage, API key handling.
- `agent.c` / `agent.h` — agent mode core, shared by the GUI and the daemon: settings and projects (`AMIGAGPT:agent/settings.json`), the system prompt (`AGENTS.md` files), skills, slash commands and subagents discovered from `AMIGAGPT:agent/` and the project's `.agents/` drawer, the built-in tools, the approval rules and the tool loop. The session history is a provider-neutral json array, described in `agent_internal.h`.
- `agentapi.c` — turns the neutral history and tool list into each chat wire format (Responses, chat/completions, Anthropic Messages, Gemini generateContent) and the answer back. A new provider format only needs a builder and a parser here. Assistant turns keep the provider's raw blocks so Claude thinking blocks and Gemini thought signatures are replayed unchanged.
- `agentdesktop.c` — screenshot of the front screen (scaled, PNG via zlib, which is why `-lz` is linked) and mouse and keyboard input through `input.device` and `keymap.library`. The pixel reading differs per platform: cybergraphics on OS3 and MorphOS with a pen-by-pen fallback for planar screens, `ReadPixelColor()` on OS4.
- `agentmcp.c` — MCP client for remote servers (Streamable HTTP, JSON-RPC), configured by `mcp.json`.
- `AgentPage.c` — the Agent tab of the main window (GUI build only; the whole file is inside `#ifndef DAEMON`).
- `ARexx.c` — ARexx port handler. The GUI listens on port `AMIGAGPT`, the daemon on `AMIGAGPTD`. Command set is documented in `README.md` (SENDMESSAGE, CREATEIMAGE, SPEAKTEXT, NEWCHAT, AGENT, LIST\*).
- `version.h` — `BUILD_NUMBER` is auto-incremented by `sed` in the Makefile on every C compile; do not hand-edit.

## Localization (gettext catalogs)

Strings are referenced as `STRING_*` symbols generated from `catalogs/AmigaGPT.pot` into `src/AmigaGPT_cat.{c,h}` by `flexcat` during the build. Per-language `.po` files live under `catalogs/<language>/`.

- `make catalog_definition` regenerates `AmigaGPT.pot` from `src/*.c` via `xgettext`.
- `make catalog` runs `flexcat` to regenerate `AmigaGPT_cat.{c,h}`, `msgmerge -U`s every `.po` against the template, and compiles each `<language>/AmigaGPT.catalog`.
- The full build target (`all`) depends on `catalog`, so adding or changing a `STRING_*` will fail at link time until the `.pot` and `_cat.{c,h}` are regenerated.
- **Localisation is part of every UI change (HARD RULE).** Whenever you add, change or remove a user-facing string, use the `update-translations` skill (`.agents/skills/update-translations/SKILL.md`) in the same task, without waiting to be asked: update `catalogs/AmigaGPT.pot`, translate the entry into every `catalogs/*/*.po`, and confirm `./build_os3.sh` passes. The work is not finished while any language is missing a translation for a string you touched. The skill delegates to the `po-translation-updater` subagent (defined in `.claude/agents/`, `.codex/agents/` and `.cursor/agents/`) and, on hosts without subagents, has you follow that agent's workflow directly. Users can also run it by hand as `/update-translations` (Claude Code, Cursor) or `$update-translations` (Codex).

## Repo-specific guidance

- Project skills live in `.agents/skills/` (Agent Skills standard). Cursor, Codex, and other compatible agents load that path directly. Claude Code loads the same files through links under `.claude/skills/`. The project skills are `build-amigagpt` and `update-translations`; general-purpose skills are installed per user, not committed here.
- When testing the AmigaGPT GUI through AmiKit/Amiberry automation, submit text from the prompt field by pressing Return. There is no need to locate and click the Send button.
- AmiKit normally finishes booting in about 10-15 seconds, so do not use a long fixed wait before checking whether it is ready. Open an Amiga Shell with Left Alt+S.
- Provider docs: prefer the project MCP servers `openaiDeveloperDocs` (OpenAI) and `xaiDocs` (xAI) over general web search. If the user has the public OpenAI or xAI docs skills installed globally, those are fine to use too.
- **Check Amiga APIs with Amiga Guru before coding (HARD RULE).** Before you write or change code that calls, adds or changes the arguments of an AmigaOS, MUI, AmiSSL, AHI, ARexx or other Amiga library function, tag, struct, constant or BOOPSI method, load the **Amiga Guru** skill (`amiga-guru`, https://github.com/sacredbanana/amiga-guru) and look up the relevant autodoc or SDK header. Do this without being asked, even for calls you think you know. Check the signature and tags on all three targets (AmigaOS 3, AmigaOS 4, MorphOS), and the minimum library version for each, and keep any needed `__AMIGAOS3__` / `__AMIGAOS4__` / `__MORPHOS__` conditionals in step. Code that touches no Amiga API (plain C logic, JSON handling, catalog text) does not need the lookup.
- Use Amiga Guru too for other AmigaOS / NDK / autodoc / SDK questions, classic Amiga book recommendations and Amiga toolchain questions. It searches a local sparse clone of `sacredbanana/AmigaSDK-gcc` and the retro-commodore.eu book catalog. Wait for its cache sync to finish before falling back to the web.
  - **If it is not installed**, stop and tell the user before doing Amiga API work, and offer to install it globally (ask first; it writes outside this repo):
    - Claude Code: `git clone https://github.com/sacredbanana/amiga-guru.git ~/.claude/skills/amiga-guru`
    - Codex / ChatGPT desktop / other Agent Skills hosts: `git clone https://github.com/sacredbanana/amiga-guru.git ~/.agents/skills/amiga-guru`
    - A restart of the agent may be needed before the skill appears. Do not install it inside this repo, and never commit its `cache/` directory.
- Building AmigaGPT itself: use the `build-amigagpt` skill — only the three `build_*.sh` scripts work in this environment.
- Before calling an MCP tool, read the installed tool descriptor and follow its schema exactly.
- For documentation answers, prefer MCP-backed official docs over general web search. If MCP is unavailable, fall back only to official provider docs pages:
  - OpenAI: `developers.openai.com`, `platform.openai.com`
  - xAI: `docs.x.ai`
- **ISO 8859-1 Amiga Latin-1 files (HARD RULE)** — Read and follow **[AMIGA-LATIN1-FILES.md](AMIGA-LATIN1-FILES.md)** before and after editing any covered file. Claude Code also loads `.claude/rules/amiga-latin1-files.md` when touching those paths. Applies to `bundle/AmigaGPT/Install-AmigaGPT`, `AmigaGPT.readme`, `AmigaGPT.guide`, all `*.c` under `src/`, and all `*.rexx` under `bundle/` and `assets/`.
