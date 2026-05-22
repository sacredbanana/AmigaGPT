#ifndef CODEFENCE_H
#define CODEFENCE_H

struct Conversation;
struct ConversationNode;

/**
 * Parse ``` fenced code blocks from raw_utf8 into node->codeblocks.
 * Placeholder numbers continue from earlier messages in conv (not per message).
 * Clears any previous blocks on this node. Incomplete trailing fences ignored.
 */
void conversationNodeParseCodeFences(struct Conversation *conv,
                                     struct ConversationNode *node);

#endif
