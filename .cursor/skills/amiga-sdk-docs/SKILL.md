---
name: amiga-sdk-docs
description: Consults the AmigaSDK-gcc reference repository for AmigaOS 3, AmigaOS 4.1, and MorphOS SDK, autodoc, include, and example-code questions. Use when the user asks about AmigaOS 3, AmigaOS 4.1, MorphOS, NDK, SDK, autodocs, headers, examples, compiler setup, or platform-specific APIs. Sync the cached project-local clone first, search the local clone before broader web search, and cite repo files when possible.
---

# Amiga SDK Docs

Use the `AmigaSDK-gcc` repository as the first source for AmigaOS 3, AmigaOS 4.1, and MorphOS SDK/autodoc/example-code questions.

Primary source repo:

- `https://github.com/sacredbanana/AmigaSDK-gcc`

Project-local cache path:

- `.cursor/cache/amigasdk-gcc`

## Default workflow

1. Sync the cached clone by running `./.cursor/skills/amiga-sdk-docs/scripts/sync-repo.sh` from the repo root.
2. Search the local cached clone for the relevant docs, headers, autodocs, README sections, examples, or platform-specific SDK content.
3. Answer from the local clone first and cite the exact local file paths when possible.
4. Only broaden to web search if the cached clone does not answer the question clearly enough.

## Sync behavior

- Wait for the sync script to complete before doing any web fallback.
- If the sync command moves to the background, monitor it until it exits successfully or fails.
- Do not treat "clone still running" as a failure condition.
- Only fall back to GitHub pages or broader web search after the sync script exits non-zero, or after it succeeds but the local clone still does not answer the question.
- If another sync is already in progress, let it finish and then use the resulting cache instead of starting web search early.

## Hard rules

- Prefer the local cached clone over broad web search for Amiga SDK questions.
- If the cache update fails, continue with the existing local clone if present and mention that it may be stale.
- If the cache does not exist and cloning fails, then use the upstream GitHub repo as the next-best source before broader web search.
- Do not commit anything under `.cursor/cache/`.

## What to search first

- `README.md`
- SDK include directories
- autodocs
- example code
- platform-specific folders such as `amigaos3`, `amigaos4`, and `morphos`
- Dockerfiles or build scripts when the question is about toolchains or SDK packaging

## When to use this skill

Use this skill for questions about:

- AmigaOS 3 SDK or NDK APIs
- AmigaOS 4.1 SDK APIs
- MorphOS SDK APIs
- autodocs, example code, or header availability
- GCC compatibility for Amiga SDK content
- platform-specific library, gadget, or subsystem references

## Fallback order

1. Local cached clone at `.cursor/cache/amigasdk-gcc`
2. Upstream GitHub repo pages for `sacredbanana/AmigaSDK-gcc`
3. Broader web search

## Notes

- The cache is project-local so teammates can use the same workflow.
- The cache path is intentionally gitignored.
