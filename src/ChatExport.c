#ifndef DAEMON

#include <dos/dos.h>
#include <libraries/asl.h>
#include <proto/dos.h>
#include <proto/exec.h>
#include <stdio.h>
#include <string.h>

#ifdef __MORPHOS__
#include <libraries/mui.h>
#include <proto/muimaster.h>
#endif

#include "AmigaGPT_cat.h"
#include "ChatExport.h"
#include "gui.h"
#include "MainWindow.h"

#define CHAT_EXPORT_DEFAULT_FILE "chat-raw.txt"
#define CHAT_EXPORT_INITIAL_NAME_MAX 96

static BOOL chatExportWriteBytes(BPTR fh, const void *data, ULONG len) {
    if (fh == 0 || data == NULL || len == 0) {
        return TRUE;
    }
    return Write(fh, data, len) == (LONG)len;
}

static BOOL chatExportWriteCStr(BPTR fh, const char *s) {
    if (s == NULL || s[0] == '\0') {
        return TRUE;
    }
    return chatExportWriteBytes(fh, s, (ULONG)strlen(s));
}

static void chatExportSanitizeFileComponent(const char *src, char *dst,
                                            ULONG dstLen) {
    ULONG i;
    ULONG out = 0;

    if (dst == NULL || dstLen == 0) {
        return;
    }
    dst[0] = '\0';
    if (src == NULL || src[0] == '\0') {
        return;
    }
    for (i = 0; src[i] != '\0' && out + 1 < dstLen; i++) {
        unsigned char c = (unsigned char)src[i];

        if ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
            (c >= '0' && c <= '9') || c == '-' || c == '_') {
            dst[out++] = (char)c;
        } else if (c == ' ' || c == '\t') {
            dst[out++] = '-';
        }
    }
    dst[out] = '\0';
    if (out == 0) {
        snprintf(dst, dstLen, "%s", "chat");
    }
}

static void chatExportDefaultFileName(struct Conversation *conversation,
                                      char *buf, ULONG bufLen) {
    char stem[CHAT_EXPORT_INITIAL_NAME_MAX];

    if (buf == NULL || bufLen == 0) {
        return;
    }
    stem[0] = '\0';
    if (conversation != NULL && conversation->name != NULL &&
        conversation->name[0] != '\0') {
        chatExportSanitizeFileComponent(conversation->name, stem, sizeof(stem));
        snprintf(buf, bufLen, "%s-raw.txt", stem);
    } else {
        snprintf(buf, bufLen, "%s", CHAT_EXPORT_DEFAULT_FILE);
    }
}

static BOOL chatExportWriteConversation(BPTR fh, struct Conversation *conv) {
    struct ConversationNode *node;
    static const char header[] =
        "# AmigaGPT chat export (raw UTF-8)\n"
        "# Each section is conversationNodeGetRaw() — not display placeholders.\n\n";

    if (fh == 0 || conv == NULL || conv->messages == NULL) {
        return FALSE;
    }

    if (!chatExportWriteBytes(fh, header, (ULONG)strlen(header))) {
        return FALSE;
    }

    if (conv->name != NULL && conv->name[0] != '\0') {
        if (!chatExportWriteCStr(fh, "# conversation: ") ||
            !chatExportWriteCStr(fh, conv->name) ||
            !chatExportWriteCStr(fh, "\n\n")) {
            return FALSE;
        }
    }

    for (node = (struct ConversationNode *)conv->messages->mlh_Head;
         node->node.mln_Succ != NULL;
         node = (struct ConversationNode *)node->node.mln_Succ) {
        UTF8 *raw;
        ULONG rawLen;

        if (!strcmp(node->role, "system")) {
            continue;
        }

        if (!chatExportWriteCStr(fh, "--- ") || !chatExportWriteCStr(fh, node->role) ||
            !chatExportWriteCStr(fh, " ---\n")) {
            return FALSE;
        }

        raw = conversationNodeGetRaw(node);
        if (raw == NULL) {
            raw = (UTF8 *)"";
        }
        rawLen = (ULONG)strlen((const char *)raw);
        if (rawLen > 0) {
            if (!chatExportWriteBytes(fh, raw, rawLen)) {
                return FALSE;
            }
        }
        if (!chatExportWriteCStr(fh, "\n\n")) {
            return FALSE;
        }
    }

    return TRUE;
}

static struct Window *chatExportAslParentWindow(void) {
    struct Window *w = NULL;

    if (mainWindowObject != NULL) {
        get(mainWindowObject, MUIA_Window, &w);
    }
    if (w == NULL) {
        w = mainWindow;
    }
    return w;
}

BOOL chatExportConversationRaw(struct Conversation *conversation,
                               struct Window *aslParent) {
    struct FileRequester *fileReq;
    char defaultName[CHAT_EXPORT_INITIAL_NAME_MAX];
    BOOL ok = FALSE;

    if (conversation == NULL) {
        displayError(STRING_ERROR_NO_CONVERSATION_EXPORT);
        return FALSE;
    }
    if (aslParent == NULL) {
        aslParent = chatExportAslParentWindow();
    }
    if (aslParent == NULL) {
        displayError(STRING_ERROR_CHAT_EXPORT_SAVE);
        return FALSE;
    }

    chatExportDefaultFileName(conversation, defaultName, sizeof(defaultName));

#ifdef __MORPHOS__
    fileReq = (struct FileRequester *)MUI_AllocAslRequestTags(
        ASL_FileRequest, ASLFR_Window, aslParent, ASLFR_PopToFront, TRUE,
        ASLFR_Activate, TRUE, ASLFR_TitleText, STRING_EXPORT_CHAT_RAW,
        ASLFR_InitialFile, defaultName, ASLFR_InitialDrawer, "SYS:",
        ASLFR_DoSaveMode, TRUE, TAG_DONE);
#else
    fileReq = (struct FileRequester *)AllocAslRequestTags(ASL_FileRequest,
                                                          TAG_END);
#endif

    if (fileReq == NULL) {
        displayError(STRING_ERROR_CHAT_EXPORT_SAVE);
        return FALSE;
    }

#ifdef __MORPHOS__
    if (MUI_AslRequestTags(fileReq, TAG_DONE)) {
#else
    if (AslRequestTags(fileReq, ASLFR_Window, aslParent, ASLFR_PopToFront, TRUE,
                       ASLFR_Activate, TRUE, ASLFR_TitleText,
                       STRING_EXPORT_CHAT_RAW, ASLFR_InitialFile, defaultName,
                       ASLFR_InitialDrawer, "SYS:", ASLFR_DoSaveMode, TRUE,
                       TAG_DONE)) {
#endif
        STRPTR savePath = fileReq->fr_Drawer;
        STRPTR saveName = fileReq->fr_File;
        ULONG fullPathLength =
            (ULONG)(strlen(savePath) + strlen(saveName) + 2);
        STRPTR fullPath = AllocVec(fullPathLength, MEMF_CLEAR);
        BPTR fh;

        if (fullPath != NULL) {
            snprintf(fullPath, fullPathLength, "%s", savePath);
            AddPart(fullPath, saveName, fullPathLength);
            fh = Open(fullPath, MODE_NEWFILE);
            if (fh != 0) {
                ok = chatExportWriteConversation(fh, conversation);
                Close(fh);
                if (!ok) {
                    displayError(STRING_ERROR_CHAT_EXPORT_SAVE);
                }
            } else {
                displayError(STRING_ERROR_CHAT_EXPORT_SAVE);
            }
            FreeVec(fullPath);
        }
    } else {
        ok = TRUE;
    }

#ifdef __MORPHOS__
    MUI_FreeAslRequest(fileReq);
#else
    FreeAslRequest(fileReq);
#endif
    return ok;
}

#endif /* !DAEMON */
