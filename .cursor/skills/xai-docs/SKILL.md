---
name: xai-docs
description: Provides current, citation-backed guidance from official xAI documentation. Use when the user asks about xAI APIs, Grok models, xAI SDKs, rate limits, pricing, tools, files, streaming, structured outputs, reasoning, image or audio capabilities, or xAI documentation in general. Prefer an installed xAI Docs MCP server when available; otherwise search and fetch only official xAI docs pages.
---

# xAI Docs

This is a Cursor-native skill for working with official xAI documentation.

## Quick start

- Prefer an installed xAI Docs MCP server if one exists in the current Cursor environment.
- Before calling any MCP tool, read its descriptor and follow its schema exactly.
- If no suitable xAI docs MCP server is available, use `WebSearch` and `WebFetch`.
- Restrict web fallback to official xAI docs domains: `docs.x.ai` and `x.ai`.

## MCP-first workflow

1. Identify whether the request is general docs lookup, model selection, API usage, SDK usage, pricing, rate limits, tools, or migration guidance.
2. If an xAI docs MCP server is installed, inspect its tool descriptors first.
3. Use the MCP server's search or list capability to find the best page.
4. Fetch the exact page needed.
5. Answer concisely with citations to the retrieved docs.

## Official xAI docs MCP shape

The official xAI docs MCP server exposes doc-search and page-fetch tools. In the current xAI server, these include:

- `search_docs` for keyword search
- `list_doc_pages` for browsing page slugs
- `get_doc_page` for fetching a page by slug

Use the actual installed server descriptors as the source of truth.

## Web fallback workflow

Use this path when no xAI docs MCP server is installed, or when the MCP server cannot answer the question well enough:

1. Search with a precise query such as `site:docs.x.ai structured outputs grok`.
2. Fetch the relevant page with `WebFetch` before citing it.
3. Cite only pages actually fetched in the current session.
4. Prefer `docs.x.ai` pages over blog posts or third-party summaries.

## Quality rules

- Treat current official xAI docs as the source of truth.
- Keep quotes short; prefer paraphrase with citations.
- If multiple official pages differ, call out the difference and cite both.
- If the docs do not cover the user's need, say so and offer the best next step.

## Answer shape

- Start with the direct answer or recommendation.
- Follow with short cited notes, steps, or tradeoffs as needed.
- If the user is comparing xAI with another provider, separate provider-specific claims clearly and cite xAI sources only for xAI claims.

## Common xAI topics

- Grok model selection
- rate limits and pricing
- streaming and sync usage
- reasoning models
- files and collections
- web search, tool use, and remote MCP tools
- REST API reference
- SDK and OpenAI-compatible usage

## Notes

- xAI hosts an official docs MCP server at `https://docs.x.ai/api/mcp`.
- This skill is designed for Cursor and does not assume a specific workspace MCP server identifier.
