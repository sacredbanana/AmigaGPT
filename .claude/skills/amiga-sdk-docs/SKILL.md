---
name: amiga-sdk-docs
description: This skill should be used when answering AmigaOS 3, AmigaOS 4.1, or MorphOS development questions — including SDK/NDK APIs, autodocs, headers, examples, compiler setup, platform-specific library APIs, classic Amiga book recommendations, and download links from the retro-commodore.eu catalog. Triggers on library API questions, book recommendations, needing download links, code examples, or any question about AmigaOS or MorphOS SDK/NDK content.
---

# Amiga SDK Docs

Provide expert answers on AmigaOS 3, AmigaOS 4, and MorphOS development by drawing on two primary sources: the sacredbanana/AmigaSDK-gcc SDK repository (local cache) and classic Amiga programming books from the retro-commodore.eu catalog.

## Primary sources

### 1. SDK and autodocs — sacredbanana/AmigaSDK-gcc

Primary source repo: `https://github.com/sacredbanana/AmigaSDK-gcc`

Project-local cache path: `.claude/skills/amiga-sdk-docs/cache/amigasdk-gcc`

### 2. Classic Amiga books — retro-commodore.eu

The retro-commodore.eu catalog hosts scanned PDFs of the official Amiga programming books. Key titles:

- **Amiga ROM Kernel Reference Manual: Libraries** — the definitive library API reference
- **Amiga ROM Kernel Reference Manual: Devices** — device and I/O reference
- **Amiga ROM Kernel Reference Manual: Includes and Autodocs** — function signatures and autodocs
- **Amiga Hardware Reference Manual** — custom chips, registers, and low-level hardware
- **Amiga User Interface Style Guide** — MUI/Intuition UI conventions
- **Amiga System Programmer's Guide** — system internals
- **ROM Kernel Reference Manual: Exec** — task management, memory, signals, ports

When a user asks for a book recommendation or download link, search retro-commodore.eu first, then cite the exact URL.

## Default workflow

1. Identify the platform (AmigaOS 3, AmigaOS 4, MorphOS) and the library or subsystem.
2. Sync the cached SDK clone by running `./.claude/skills/amiga-sdk-docs/scripts/sync-repo.sh` from the repo root.
3. For code examples, autodocs, or header questions — search the local cached clone first and cite exact local file paths.
4. For book references, download links, or conceptual questions — search retro-commodore.eu.
5. Answer with precise citations: local file path from the SDK cache, or URL from retro-commodore.eu.
6. Include a minimal, compilable C example when the question is about API usage.
7. Only broaden to web search if neither the SDK cache nor retro-commodore.eu answers the question.

## Sync behavior

- Wait for the sync script to complete before doing any web fallback.
- If the sync command moves to the background, monitor it until it exits successfully or fails.
- Do not treat "clone still running" as a failure condition.
- Only fall back to GitHub pages or broader web search after the sync script exits non-zero, or after it succeeds but the local clone still does not answer the question.
- If another sync is already in progress, let it finish and use the resulting cache instead of starting web search early.

## Answering library API questions

When a user asks how to use a specific library function:

1. Look up the autodoc entry in the SDK cache under the relevant platform folder.
2. Show the function signature, parameters, and return value.
3. Note platform differences (OS3 vs OS4 vs MorphOS) when they exist.
4. Provide a short usage example in C.

## What to search first in the SDK cache

- `README.md`
- SDK include directories
- autodocs
- example code
- platform-specific folders such as `amigaos3`, `amigaos4`, and `morphos`
- Dockerfiles or build scripts when the question is about toolchains or SDK packaging

## Platform notes

| Platform | Compiler | Key macro |
|---|---|---|
| AmigaOS 3 | m68k-amigaos-gcc | `__AMIGAOS3__` |
| AmigaOS 4 | ppc-amigaos-gcc | `__AMIGAOS4__` |
| MorphOS | ppc-morphos-gcc | `__MORPHOS__` |

AmigaGPT targets all three platforms from the same source tree. When answering platform-specific questions, note which macro guards apply.

## Hard rules

- Prefer the local cached clone over broad web search for SDK questions.
- If the cache update fails, continue with the existing local clone if present and mention that it may be stale.
- If the cache does not exist and cloning fails, use the upstream GitHub repo before broader web search.
- Do not commit anything under `.cursor/cache/`.

## Fallback order

1. Local SDK cache at `.claude/skills/amiga-sdk-docs/cache/amigasdk-gcc`
2. retro-commodore.eu book catalog
3. Upstream GitHub repo: `https://github.com/sacredbanana/AmigaSDK-gcc`
4. Broader web search (last resort)

## Notes

- The cache is project-local so teammates can use the same workflow.
- The cache path is intentionally gitignored.
