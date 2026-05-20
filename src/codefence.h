#ifndef CODEFENCE_H
#define CODEFENCE_H

struct ConversationNode;

/**
 * Parse ``` fenced code blocks from raw_utf8 into node->codeblocks.
 * Clears any previous blocks. Safe to call on every message (user/assistant).
 * Incomplete trailing fences (no closing ```) are ignored.
 * When at least one complete block exists, sets display_text (chat) with
 * fenced regions replaced by STRING_CHAT_CODEBLOCK_PLACEHOLDER (catalog).
 */
void conversationNodeParseCodeFences(struct ConversationNode *node);

#endif
