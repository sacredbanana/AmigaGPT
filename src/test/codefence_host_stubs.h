#ifndef CODEFENCE_HOST_STUBS_H
#define CODEFENCE_HOST_STUBS_H

/*
 * Minimal Amiga/Exec stubs so src/codefence.c can be host-tested with gcc.
 */
#include <stddef.h>
#include <stdlib.h>
#include <string.h>

typedef unsigned char UBYTE;
typedef unsigned long ULONG;
typedef char *STRPTR;
typedef char *UTF8;
typedef int BOOL;
typedef void *APTR;

#define TRUE 1
#define FALSE 0
#define MEMF_CLEAR 0x0001

#define STRING_CHAT_CODEBLOCK_PLACEHOLDER ((UBYTE *)"[Codeblock %ld]\n")

struct MinNode {
    struct MinNode *mln_Succ;
    struct MinNode *mln_Pred;
};

struct MinList {
    struct MinNode *mlh_Head;
    struct MinNode *mlh_Tail;
    struct MinNode *mlh_TailPred;
};

struct Node {
    struct Node *ln_Succ;
    struct Node *ln_Pred;
};

struct List {
    struct Node *lh_Head;
    struct Node *lh_Tail;
    struct Node *lh_TailPred;
};

struct AICodeBlock {
    struct MinNode node;
    ULONG index;
    STRPTR language;
    STRPTR raw_code;
    ULONG code_length;
};

struct ConversationNode {
    struct MinNode node;
    UBYTE role[10];
    UTF8 *raw_utf8;
    ULONG raw_length;
    UTF8 *display_text;
    struct MinList *codeblocks;
};

struct Conversation {
    struct MinList *messages;
    UTF8 *name;
    STRPTR name_list_display;
    UTF8 *system;
};

static inline APTR AllocVec(ULONG byteSize, ULONG requirements) {
    (void)requirements;
    return malloc(byteSize);
}

static inline void FreeVec(APTR memoryBlock) {
    free(memoryBlock);
}

static inline void CopyMem(APTR source, APTR destination, ULONG size) {
    memcpy(destination, source, (size_t)size);
}

static inline void init_minlist(struct MinList *list) {
    list->mlh_Tail = NULL;
    list->mlh_Head = (struct MinNode *)&list->mlh_Tail;
    list->mlh_TailPred = (struct MinNode *)&list->mlh_Head;
}

static inline void AddTail(struct List *list, struct Node *node) {
    struct MinList *ml = (struct MinList *)list;
    struct MinNode *mn = (struct MinNode *)node;
    struct MinNode *tailpred = ml->mlh_TailPred;

    mn->mln_Succ = (struct MinNode *)&ml->mlh_Tail;
    mn->mln_Pred = tailpred;
    tailpred->mln_Succ = mn;
    ml->mlh_TailPred = mn;
}

static inline struct Node *RemHead(struct List *list) {
    struct MinList *ml = (struct MinList *)list;
    struct MinNode *head = ml->mlh_Head;

    if (head == (struct MinNode *)&ml->mlh_Tail) {
        return NULL;
    }
    ml->mlh_Head = head->mln_Succ;
    head->mln_Succ->mln_Pred = (struct MinNode *)&ml->mlh_Head;
    if (ml->mlh_Tail == NULL) {
        ml->mlh_TailPred = (struct MinNode *)&ml->mlh_Head;
    }
    return (struct Node *)head;
}

#endif
