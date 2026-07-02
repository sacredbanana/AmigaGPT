---
name: openai-docs
description: This skill should be used when the user asks about OpenAI APIs, models, SDKs, Responses API, Chat Completions, Realtime, Agents SDK, Apps SDK, Codex, GPT-5.4 upgrades, prompt-upgrade guidance, or choosing the latest OpenAI model. Prefer an installed OpenAI docs MCP server when available; otherwise search and fetch only official OpenAI docs pages.
---

# OpenAI Docs

Provides current, citation-backed guidance from official OpenAI documentation.

## Quick start

- Prefer an installed OpenAI docs MCP server if one exists in the current Claude Code session.
- Before calling any MCP tool, read its descriptor and follow its schema exactly.
- If no suitable OpenAI docs MCP server is available, use `WebSearch` and `WebFetch`.
- Restrict web fallback to official OpenAI domains: `developers.openai.com` and `platform.openai.com`.
- Load only the relevant file from `references/` for model selection or GPT-5.4 upgrade work.

## MCP-first workflow

1. Identify whether the request is general docs lookup, model selection, GPT-5.4 upgrade planning, or GPT-5.4 prompt-upgrade work.
2. If an OpenAI docs MCP server is installed, inspect its tool descriptors first.
3. Use the MCP server's search or list capability to find the best page.
4. Fetch the exact page or section needed.
5. Answer concisely with citations to the retrieved docs.

## Web fallback workflow

Use this path when no OpenAI docs MCP server is installed, or when the MCP server cannot answer the question well enough:

1. Search with a precise query such as `site:developers.openai.com Responses API background mode`.
2. Fetch the relevant page with `WebFetch` before citing it.
3. Cite only pages actually fetched in the current session.
4. Prefer `developers.openai.com`; use `platform.openai.com` only when that is where the relevant page lives.

## Request routing

- Model selection: read `references/latest-model.md`, then verify every recommendation against current OpenAI docs before answering.
- Explicit GPT-5.4 upgrade request: read `references/upgrading-to-gpt-5p4.md`, then verify the guidance against current OpenAI docs.
- GPT-5.4 prompt rewrite or prompt-behavior change: also read `references/gpt-5p4-prompting-guide.md`.

## Quality rules

- Treat current official OpenAI docs as the source of truth.
- Use bundled reference files only as helper context.
- Keep quotes short; prefer paraphrase with citations.
- If multiple official pages differ, call out the difference and cite both.
- If the docs do not cover the user's need, say so and offer the best next step.

## Answer shape

- Start with the direct answer or recommendation.
- Follow with short cited notes, steps, or tradeoffs as needed.
- For GPT-5.4 upgrade reviews, make the result explicit per usage site:
  - current model usage
  - recommended model string
  - starting reasoning recommendation
  - prompt updates
  - `phase` assessment when relevant
  - compatibility status
  - validation plan

## Common OpenAI product areas

- Apps SDK
- Responses API
- Chat Completions API
- Realtime API
- Agents SDK
- Codex
- gpt-oss
- image, audio, speech, transcription, embeddings, moderation, and video models

## Reference files

- **`references/latest-model.md`** — curated current model map; verify against live docs before citing
- **`references/upgrading-to-gpt-5p4.md`** — step-by-step GPT-5.4 upgrade workflow
- **`references/gpt-5p4-prompting-guide.md`** — prompt rewrite patterns for GPT-5.4 upgrades
