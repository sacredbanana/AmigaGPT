# Agent Notes

## Provider docs

- For OpenAI questions, prefer the project MCP server `openaiDeveloperDocs` and the project skill `openai-docs`.
- For xAI questions, prefer the project MCP server `xaiDocs` and the project skill `xai-docs`.
- For AmigaOS 3, AmigaOS 4.1, MorphOS, SDK, NDK, autodoc, include, or example-code questions, prefer the project skill `amiga-sdk-docs`.
- For `amiga-sdk-docs`, sync and search the local cache at `.cursor/cache/amigasdk-gcc` before broader web search.
- For `amiga-sdk-docs`, wait for the sync script to finish successfully or fail before any fallback to GitHub pages or broader web search.
- Before calling an MCP tool, read the installed tool descriptor and follow its schema exactly.
- For documentation answers, prefer MCP-backed official docs over general web search.
- If MCP is unavailable, fall back only to official provider docs pages:
  - OpenAI: `developers.openai.com`, `platform.openai.com`
  - xAI: `docs.x.ai`
