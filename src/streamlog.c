#include <proto/dos.h>
#include <proto/exec.h>
#include <stdio.h>
#include <string.h>
#include "config.h"
#include "streamlog.h"

#ifdef __MORPHOS__
#ifndef DAEMON
#include <clib/debug_protos.h>
#endif
#endif

#define STREAMLOG_LINE_MAX 512

#ifdef DAEMON

void streamLogSyncFromConfig(void) {}

BOOL streamLogIsEnabled(void) { return FALSE; }

void streamLogChatEnd(CONST_STRPTR outcome, ULONG messageLen, BOOL completedOk,
                      BOOL truncated, CONST_STRPTR lastSseSnippet) {
    (void)outcome;
    (void)messageLen;
    (void)completedOk;
    (void)truncated;
    (void)lastSseSnippet;
}

void streamLogApiError(CONST_STRPTR kind, CONST_STRPTR detail) {
    (void)kind;
    (void)detail;
}

void streamLogUtf8(CONST_STRPTR detail) { (void)detail; }

void streamLogBootPhase(CONST_STRPTR phase) { (void)phase; }

void streamLogLifecycle(CONST_STRPTR phase) { (void)phase; }

void streamLogShutdownPhase(CONST_STRPTR phase) { (void)phase; }

#else

static BOOL streamLogEnabled = FALSE;
static BOOL streamLogLifecycleEnabled = FALSE;

static void streamLogSanitizeSnippet(STRPTR dest, ULONG destSize,
                                     CONST_STRPTR src) {
    ULONG i;
    ULONG out = 0;

    if (dest == NULL || destSize < 2) {
        return;
    }
    dest[0] = '\0';
    if (src == NULL) {
        return;
    }
    for (i = 0; src[i] != '\0' && out + 1 < destSize; i++) {
        UBYTE c = (UBYTE)src[i];
        if (c == '\r' || c == '\n' || c == '\t') {
            if (out + 0 < destSize - 1) {
                dest[out++] = '|';
            }
            continue;
        }
        if (c < 32) {
            continue;
        }
        dest[out++] = (UBYTE)c;
    }
    dest[out] = '\0';
}

static BOOL streamLogLooksSensitive(CONST_STRPTR text) {
    if (text == NULL) {
        return FALSE;
    }
    if (strstr(text, "api_key") != NULL || strstr(text, "apikey") != NULL ||
        strstr(text, "Authorization") != NULL ||
        strstr(text, "authorization") != NULL ||
        strstr(text, "Bearer ") != NULL || strstr(text, "bearer ") != NULL) {
        return TRUE;
    }
    return FALSE;
}

static void streamLogAppendFile(CONST_STRPTR path, CONST_STRPTR line) {
    BPTR fh;
    ULONG len;

    if (!streamLogEnabled || path == NULL || line == NULL || line[0] == '\0') {
        return;
    }
    len = (ULONG)strlen(line);
    fh = Open((STRPTR)path, MODE_READWRITE);
    if (fh == 0) {
        fh = Open((STRPTR)path, MODE_NEWFILE);
    }
    if (fh == 0) {
        return;
    }
    Seek(fh, 0, OFFSET_END);
    Write(fh, (APTR)line, len);
    Write(fh, (APTR)"\n", 1);
    Close(fh);
}

static void streamLogAppendAlways(CONST_STRPTR path, CONST_STRPTR line) {
    BPTR fh;
    ULONG len;

    if (path == NULL || line == NULL || line[0] == '\0') {
        return;
    }
    len = (ULONG)strlen(line);
    fh = Open((STRPTR)path, MODE_READWRITE);
    if (fh == 0) {
        fh = Open((STRPTR)path, MODE_NEWFILE);
    }
    if (fh == 0) {
        return;
    }
    Seek(fh, 0, OFFSET_END);
    Write(fh, (APTR)line, len);
    Write(fh, (APTR)"\n", 1);
    Close(fh);
}

static void streamLogEmit(CONST_STRPTR path, CONST_STRPTR line) {
    if (!streamLogEnabled || line == NULL) {
        return;
    }
    streamLogAppendFile(path, line);
#ifdef __MORPHOS__
    KPrintF("[AmigaGPT] %s\n", line);
#endif
}

void streamLogSyncFromConfig(void) {
    BOOL wasEnabled = streamLogEnabled;
    BOOL wasLifecycleEnabled = streamLogLifecycleEnabled;

    streamLogEnabled = (BOOL)config.debugStreamLog;
    streamLogLifecycleEnabled = (BOOL)config.debugLifecycleLog;
    if (streamLogEnabled && !wasEnabled) {
        streamLogEmit(STREAMLOG_STREAM_PATH, "debug logging enabled");
    }
    if (streamLogLifecycleEnabled && !wasLifecycleEnabled) {
        streamLogLifecycle("lifecycle logging enabled");
    }
}

BOOL streamLogIsEnabled(void) { return streamLogEnabled; }

void streamLogChatEnd(CONST_STRPTR outcome, ULONG messageLen,
                      BOOL completedOk, BOOL truncated,
                      CONST_STRPTR lastSseSnippet) {
    UBYTE line[STREAMLOG_LINE_MAX];
    UBYTE snippet[STREAMLOG_SSE_SNIPPET_MAX + 1];

    if (!streamLogEnabled) {
        return;
    }
    streamLogSanitizeSnippet((STRPTR)snippet, sizeof(snippet), lastSseSnippet);
    if (streamLogLooksSensitive((STRPTR)snippet)) {
        snprintf((STRPTR)line, sizeof(line),
                 "stream end outcome=%s len=%lu completed=%u truncated=%u "
                 "sse=(redacted)",
                 outcome != NULL ? outcome : "?",
                 (unsigned long)messageLen, (unsigned)completedOk,
                 (unsigned)truncated);
    } else {
        snprintf((STRPTR)line, sizeof(line),
                 "stream end outcome=%s len=%lu completed=%u truncated=%u "
                 "sse=%s",
                 outcome != NULL ? outcome : "?",
                 (unsigned long)messageLen, (unsigned)completedOk,
                 (unsigned)truncated, snippet);
    }
    streamLogEmit(STREAMLOG_STREAM_PATH, (STRPTR)line);
}

void streamLogApiError(CONST_STRPTR kind, CONST_STRPTR detail) {
    UBYTE line[STREAMLOG_LINE_MAX];
    UBYTE detailBuf[STREAMLOG_SSE_SNIPPET_MAX + 1];

    if (!streamLogEnabled) {
        return;
    }
    streamLogSanitizeSnippet((STRPTR)detailBuf, sizeof(detailBuf), detail);
    if (streamLogLooksSensitive((STRPTR)detailBuf)) {
        snprintf((STRPTR)line, sizeof(line), "api_error kind=%s detail=(redacted)",
                 kind != NULL ? kind : "?");
    } else {
        snprintf((STRPTR)line, sizeof(line), "api_error kind=%s detail=%s",
                 kind != NULL ? kind : "?", detailBuf);
    }
    streamLogEmit(STREAMLOG_STREAM_PATH, (STRPTR)line);
}

void streamLogUtf8(CONST_STRPTR detail) {
    UBYTE line[STREAMLOG_LINE_MAX];

    if (!streamLogEnabled || detail == NULL) {
        return;
    }
    snprintf((STRPTR)line, sizeof(line), "utf8 %s", detail);
    streamLogEmit(STREAMLOG_UTF8_PATH, (STRPTR)line);
}

void streamLogBootPhase(CONST_STRPTR phase) {
    UBYTE line[STREAMLOG_LINE_MAX];

    if (!streamLogEnabled || phase == NULL) {
        return;
    }
    snprintf((STRPTR)line, sizeof(line), "boot %s", phase);
    streamLogEmit(STREAMLOG_STREAM_PATH, (STRPTR)line);
}

void streamLogLifecycle(CONST_STRPTR phase) {
    struct DateStamp ds;
    APTR currentTask;
    UBYTE line[STREAMLOG_LINE_MAX];

    if (!streamLogLifecycleEnabled || phase == NULL) {
        return;
    }
    DateStamp(&ds);
    currentTask = (APTR)FindTask(NULL);
    snprintf((STRPTR)line, sizeof(line),
             "lifecycle ds=%lu:%lu:%lu task=%p %s", (unsigned long)ds.ds_Days,
             (unsigned long)ds.ds_Minute, (unsigned long)ds.ds_Tick, currentTask,
             phase);
    streamLogAppendAlways(STREAMLOG_LIFECYCLE_PATH, (STRPTR)line);
    streamLogAppendAlways(STREAMLOG_LIFECYCLE_MIRROR_PATH, (STRPTR)line);
#ifdef __MORPHOS__
    KPrintF("[AmigaGPT lifecycle] %s\n", line);
#endif
}

void streamLogShutdownPhase(CONST_STRPTR phase) {
#ifdef __MORPHOS__
    BPTR fh;
    ULONG len;

    if (phase == NULL || phase[0] == '\0') {
        return;
    }
    len = (ULONG)strlen(phase);
    fh = Open((STRPTR)STREAMLOG_SHUTDOWN_LAST_PATH, MODE_NEWFILE);
    if (fh == 0) {
        return;
    }
    Write(fh, (APTR)phase, len);
    Write(fh, (APTR)"\n", 1);
    Close(fh);
#else
    (void)phase;
#endif
}

#endif /* DAEMON */
