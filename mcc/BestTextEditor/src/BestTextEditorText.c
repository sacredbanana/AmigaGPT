/*
 * BestTextEditor - Unicode text
 *
 * The document is UTF-8. This file decodes it, splits it into clusters (a
 * character with its combining marks, or a whole emoji sequence), decides
 * how each cluster is drawn and converts between UTF-8 and the system
 * codeset at the edges of the class.
 *
 * A cluster is drawn with the Amiga font when the system codeset has the
 * character, as a look-alike in the system codeset (curly quotes, dashes,
 * arrows...) when it has a common substitute, as a colour emoji from
 * BestTextEditor.emoji, and as a box when nothing fits.
 *
 * The codeset table, the emoji index and the decoded emoji pixels belong to
 * the library and are shared by every object; the bitmaps and pens an emoji
 * needs on a screen belong to the object.
 */
#include <dos/dos.h>
#include <dos/dosextens.h>
#include <exec/memory.h>
#include <exec/semaphores.h>
#include <graphics/gfx.h>
#include <graphics/rastport.h>
#include <graphics/view.h>
#include <intuition/screens.h>
#include <libraries/codesets.h>
#include <proto/codesets.h>
#include <proto/dos.h>
#include <proto/exec.h>
#include <proto/graphics.h>
#if !defined(__AMIGAOS4__)
#include <cybergraphx/cybergraphics.h>
#include <proto/cybergraphics.h>
#endif
#include <proto/muimaster.h>
#include <string.h>
#include "BestTextEditorPrivate.h"

/* ------------------------------------------------------------------------ */
/* System codeset                                                            */
/* ------------------------------------------------------------------------ */

struct RTEUniCode {
    ULONG uni;
    UBYTE code;
    UBYTE pad[3];
};

static ULONG rteSysUni[256];             /* system byte -> Unicode */
static struct RTEUniCode rteUniSys[256]; /* sorted by Unicode */
static UWORD rteUniSysCount;
static BOOL rteSystemUTF8;               /* the system codeset is UTF-8 */

static void rteSetLatin1(void) {
    UWORD i;
    for (i = 0; i < 256; i++)
        rteSysUni[i] = i;
}

static void rteSortCodeset(void) {
    UWORD i, j;
    rteUniSysCount = 0;
    for (i = 0x80; i < 256; i++) {
        ULONG u = rteSysUni[i];
        struct RTEUniCode e;
        if (u < 0x80 || u == 0xfffd)
            continue;
        e.uni = u;
        e.code = (UBYTE)i;
        /* insertion sort: at most 128 entries, done once */
        for (j = rteUniSysCount; j > 0 && rteUniSys[j - 1].uni > u; j--)
            rteUniSys[j] = rteUniSys[j - 1];
        rteUniSys[j] = e;
        rteUniSysCount++;
    }
}

/**
 * Read the system codeset from codesets.library once. Without the library
 * the system codeset is taken to be ISO-8859-1.
 */
static void rteInitCodeset(void) {
    struct Library *CodesetsBase;
#if defined(__AMIGAOS4__)
    struct CodesetsIFace *ICodesets;
#endif
    struct codeset *cs;
    UWORD i;

    rteSetLatin1();
    rteSystemUTF8 = FALSE;
    CodesetsBase = OpenLibrary(CODESETSNAME, CODESETSVER);
    if (CodesetsBase != NULL) {
#if defined(__AMIGAOS4__)
        ICodesets = (struct CodesetsIFace *)GetInterface(CodesetsBase, "main",
                                                         1, NULL);
        if (ICodesets != NULL) {
#endif
            cs = CodesetsFindA(NULL, NULL);
            if (cs != NULL) {
                if (cs->MIBenum == 106 ||
                    (cs->name != NULL && (strcmp(cs->name, "UTF-8") == 0 ||
                                          strcmp(cs->name, "utf-8") == 0))) {
                    rteSystemUTF8 = TRUE;
                } else {
                    for (i = 0; i < 256; i++) {
                        UBYTE code = cs->table[i].code;
                        ULONG u = cs->table[i].ucs4;
                        if (u == 0 && code != 0)
                            u = 0xfffd;
                        rteSysUni[code] = u;
                    }
                    /* ASCII stays ASCII */
                    for (i = 0; i < 0x80; i++)
                        rteSysUni[i] = i;
                }
            }
#if defined(__AMIGAOS4__)
            DropInterface((struct Interface *)ICodesets);
        }
#endif
        CloseLibrary(CodesetsBase);
    }
    rteSortCodeset();
}

/** The system codeset byte for a character, or -1 */
static LONG rteUniToSys(ULONG u) {
    LONG lo = 0, hi = (LONG)rteUniSysCount - 1;
    if (u < 0x80)
        return (LONG)u;
    while (lo <= hi) {
        LONG mid = (lo + hi) / 2;
        if (rteUniSys[mid].uni == u)
            return rteUniSys[mid].code;
        if (rteUniSys[mid].uni < u)
            lo = mid + 1;
        else
            hi = mid - 1;
    }
    return -1;
}

/* ------------------------------------------------------------------------ */
/* UTF-8                                                                     */
/* ------------------------------------------------------------------------ */

/**
 * Decode the character at s. A byte that does not start a valid UTF-8
 * sequence is read as an ISO-8859-1 character, so text that is not UTF-8
 * still shows.
 * @return the number of bytes used (at least 1)
 */
ULONG rteDecodeUTF8(CONST_STRPTR s, ULONG len, ULONG *cp) {
    const UBYTE *u = (const UBYTE *)s;
    UBYTE c = u[0];
    ULONG need, v, min, i;

    if (c < 0x80 || len == 0) {
        *cp = c;
        return 1;
    }
    if (c >= 0xc2 && c <= 0xdf) {
        need = 1;
        v = c & 0x1f;
        min = 0x80;
    } else if (c >= 0xe0 && c <= 0xef) {
        need = 2;
        v = c & 0x0f;
        min = 0x800;
    } else if (c >= 0xf0 && c <= 0xf4) {
        need = 3;
        v = c & 0x07;
        min = 0x10000;
    } else {
        *cp = c;
        return 1;
    }
    if (need >= len) {
        *cp = c;
        return 1;
    }
    for (i = 1; i <= need; i++) {
        if ((u[i] & 0xc0) != 0x80) {
            *cp = c;
            return 1;
        }
        v = (v << 6) | (u[i] & 0x3f);
    }
    if (v < min || v > 0x10ffff || (v >= 0xd800 && v <= 0xdfff)) {
        *cp = c;
        return 1;
    }
    *cp = v;
    return need + 1;
}

ULONG rteEncodeUTF8(ULONG cp, UBYTE *out) {
    if (cp < 0x80) {
        out[0] = (UBYTE)cp;
        return 1;
    }
    if (cp < 0x800) {
        out[0] = (UBYTE)(0xc0 | (cp >> 6));
        out[1] = (UBYTE)(0x80 | (cp & 0x3f));
        return 2;
    }
    if (cp < 0x10000) {
        out[0] = (UBYTE)(0xe0 | (cp >> 12));
        out[1] = (UBYTE)(0x80 | ((cp >> 6) & 0x3f));
        out[2] = (UBYTE)(0x80 | (cp & 0x3f));
        return 3;
    }
    out[0] = (UBYTE)(0xf0 | (cp >> 18));
    out[1] = (UBYTE)(0x80 | ((cp >> 12) & 0x3f));
    out[2] = (UBYTE)(0x80 | ((cp >> 6) & 0x3f));
    out[3] = (UBYTE)(0x80 | (cp & 0x3f));
    return 4;
}

/* ------------------------------------------------------------------------ */
/* Look-alikes                                                               */
/* ------------------------------------------------------------------------ */

/* Characters common in modern text that most Amiga codesets lack, with
 * what is drawn (and exported) instead. The replacements are UTF-8 and go
 * through the system codeset again. Entry 0 is the empty replacement used
 * for invisible characters. Sorted by code point. */
struct RTETranslit {
    ULONG cp;
    const char *text;
};

static const struct RTETranslit rteTranslits[] = {
    {0x0000, ""},
    {0x2002, " "},   {0x2003, " "},   {0x2004, " "},    {0x2005, " "},
    {0x2006, " "},   {0x2007, " "},   {0x2008, " "},    {0x2009, " "},
    {0x200a, " "},   {0x200b, ""},    {0x200c, ""},     {0x200d, ""},
    {0x200e, ""},    {0x200f, ""},    {0x2010, "-"},    {0x2011, "-"},
    {0x2012, "-"},   {0x2013, "-"},   {0x2014, "-"},    {0x2015, "-"},
    {0x2016, "|"},   {0x2018, "'"},   {0x2019, "'"},    {0x201a, ","},
    {0x201b, "'"},   {0x201c, "\""},  {0x201d, "\""},   {0x201e, "\""},
    {0x201f, "\""},  {0x2022, "\xc2\xb7"},              {0x2023, ">"},
    {0x2024, "."},   {0x2025, ".."},  {0x2026, "..."},  {0x2027, "\xc2\xb7"},
    {0x2028, ""},    {0x2029, ""},    {0x202f, " "},    {0x2030, "%."},
    {0x2032, "'"},   {0x2033, "\""},  {0x2039, "<"},    {0x203a, ">"},
    {0x2043, "-"},   {0x2044, "/"},   {0x205f, " "},    {0x2060, ""},
    {0x20ac, "EUR"}, {0x2116, "No"},  {0x2122, "TM"},   {0x2190, "<-"},
    {0x2191, "^"},   {0x2192, "->"},  {0x2193, "v"},    {0x2194, "<->"},
    {0x21d0, "<="},  {0x21d2, "=>"},  {0x21d4, "<=>"},  {0x2212, "-"},
    {0x2215, "/"},   {0x2217, "*"},   {0x2219, "\xc2\xb7"},
    {0x221e, "oo"},  {0x2223, "|"},   {0x2248, "~"},    {0x2260, "!="},
    {0x2261, "=="},  {0x2264, "<="},  {0x2265, ">="},   {0x2500, "-"},
    {0x2501, "-"},   {0x2502, "|"},   {0x2503, "|"},    {0x250c, "+"},
    {0x2510, "+"},   {0x2514, "+"},   {0x2518, "+"},    {0x251c, "+"},
    {0x2524, "+"},   {0x252c, "+"},   {0x2534, "+"},    {0x253c, "+"},
    {0x2550, "="},   {0x2551, "|"},   {0x2554, "+"},    {0x2557, "+"},
    {0x255a, "+"},   {0x255d, "+"},   {0x25cb, "o"},    {0x25cf, "*"},
    {0x2713, "v"},   {0x2717, "x"},   {0x27e8, "<"},    {0x27e9, ">"},
    {0xfb01, "fi"},  {0xfb02, "fl"},  {0xfe0e, ""},     {0xfe0f, ""},
    {0xfeff, ""},
};

#define RTE_NUM_TRANSLITS (sizeof(rteTranslits) / sizeof(rteTranslits[0]))

/** Index of the look-alike for a character, or -1 */
static LONG rteFindTranslit(ULONG cp) {
    LONG lo = 1, hi = (LONG)RTE_NUM_TRANSLITS - 1;
    while (lo <= hi) {
        LONG mid = (lo + hi) / 2;
        if (rteTranslits[mid].cp == cp)
            return mid;
        if (rteTranslits[mid].cp < cp)
            lo = mid + 1;
        else
            hi = mid - 1;
    }
    return -1;
}

/** Marks that combine with the character before them */
static BOOL rteIsCombining(ULONG cp) {
    return (cp >= 0x0300 && cp <= 0x036f) || (cp >= 0x1ab0 && cp <= 0x1aff) ||
           (cp >= 0x1dc0 && cp <= 0x1dff) || (cp >= 0x20d0 && cp <= 0x20ff) ||
           (cp >= 0xfe20 && cp <= 0xfe2f) || (cp >= 0xfe00 && cp <= 0xfe0f) ||
           (cp >= 0xe0100 && cp <= 0xe01ef);
}

/**
 * The precomposed Latin-1 letter for a letter followed by a combining mark
 * (text from some systems is decomposed), or 0
 */
static ULONG rteCompose(ULONG base, ULONG mark) {
    static const struct {
        UWORD mark;
        const char *pairs; /* base letter, then the result - 0xc0 */
    } table[] = {
        {0x300, "A\x00" "E\x08" "I\x0c" "O\x12" "U\x19" "a\x20" "e\x28"
                "i\x2c" "o\x32" "u\x39"},
        {0x301, "A\x01" "E\x09" "I\x0d" "O\x13" "U\x1a" "Y\x1d" "a\x21"
                "e\x29" "i\x2d" "o\x33" "u\x3a" "y\x3d"},
        {0x302, "A\x02" "E\x0a" "I\x0e" "O\x14" "U\x1b" "a\x22" "e\x2a"
                "i\x2e" "o\x34" "u\x3b"},
        {0x303, "A\x03" "N\x11" "O\x15" "a\x23" "n\x31" "o\x35"},
        {0x308, "A\x04" "E\x0b" "I\x0f" "O\x16" "U\x1c" "a\x24" "e\x2b"
                "i\x2f" "o\x36" "u\x3c" "y\x3f"},
        {0x30a, "A\x05" "a\x25"},
        {0x327, "C\x07" "c\x27"},
    };
    ULONG t;
    if (base >= 0x80)
        return 0;
    for (t = 0; t < sizeof(table) / sizeof(table[0]); t++) {
        const char *p;
        if (table[t].mark != mark)
            continue;
        /* only the letters are tested for the end: A + grave stores 0 */
        for (p = table[t].pairs; *p != '\0'; p += 2) {
            if ((ULONG)(UBYTE)p[0] == base)
                return 0xc0 + (UBYTE)p[1];
        }
        return 0;
    }
    return 0;
}

/* ------------------------------------------------------------------------ */
/* Emoji data                                                                */
/* ------------------------------------------------------------------------ */

#define RTE_EMOJI_MAXSEQ 12

static struct SignalSemaphore rteEmojiLock;
static BOOL rteEmojiTried;
static BOOL rteEmojiLoaded;
static BPTR rteEmojiFile;
static UBYTE *rteEmojiHead;      /* header, palette and index */
static ULONG rteEmojiHeadLen;
static const UBYTE **rteEmojiRecs;
static ULONG *rteEmojiOffsets;   /* count + 1 data offsets */
static UBYTE **rteEmojiPixels;   /* decoded pixels, NULL until needed */
static const UBYTE *rteEmojiPalette;
static UWORD rteEmojiPixelSize;
static UWORD rteEmojiCount;
static UWORD rteEmojiColors;
static ULONG rteEmojiDataOffset;
static UBYTE rteEmojiMaxLen;

static CONST_STRPTR const rteEmojiPaths[] = {
    "MUI:Libs/MUI/BestTextEditor.emoji",
    "SYS:Classes/MUI/BestTextEditor.emoji",
    "MOSSYS:Classes/MUI/BestTextEditor.emoji",
    "LIBS:MUI/BestTextEditor.emoji",
    "PROGDIR:BestTextEditor.emoji",
    NULL};

static BOOL rteFileSeek(BPTR fh, ULONG pos) {
#if defined(__AMIGAOS4__)
    return ChangeFilePosition(fh, (int64)pos, OFFSET_BEGINNING) != 0;
#else
    return Seek(fh, (LONG)pos, OFFSET_BEGINNING) != -1;
#endif
}

static ULONG rteFileSize(BPTR fh) {
#if defined(__AMIGAOS4__)
    int64 size = GetFileSize(fh);
    return size < 0 ? 0 : (ULONG)size;
#else
    LONG size;
    Seek(fh, 0, OFFSET_END);
    size = Seek(fh, 0, OFFSET_BEGINNING);
    return size < 0 ? 0 : (ULONG)size;
#endif
}

static ULONG rteGet16(const UBYTE *p) { return ((ULONG)p[0] << 8) | p[1]; }

static ULONG rteGet24(const UBYTE *p) {
    return ((ULONG)p[0] << 16) | ((ULONG)p[1] << 8) | p[2];
}

static ULONG rteGet32(const UBYTE *p) {
    return ((ULONG)p[0] << 24) | ((ULONG)p[1] << 16) | ((ULONG)p[2] << 8) |
           p[3];
}

static void rteUnloadEmoji(void) {
    UWORD i;
    if (rteEmojiPixels != NULL) {
        for (i = 0; i < rteEmojiCount; i++) {
            if (rteEmojiPixels[i] != NULL)
                FreeVec(rteEmojiPixels[i]);
        }
        FreeVec(rteEmojiPixels);
    }
    if (rteEmojiRecs != NULL)
        FreeVec((APTR)rteEmojiRecs);
    if (rteEmojiOffsets != NULL)
        FreeVec(rteEmojiOffsets);
    if (rteEmojiHead != NULL)
        FreeVec(rteEmojiHead);
    if (rteEmojiFile != 0)
        Close(rteEmojiFile);
    rteEmojiPixels = NULL;
    rteEmojiRecs = NULL;
    rteEmojiOffsets = NULL;
    rteEmojiHead = NULL;
    rteEmojiFile = 0;
    rteEmojiLoaded = FALSE;
}

/** Open the emoji file and read its index. Called with the lock held. */
static void rteLoadEmoji(void) {
    struct Process *me = (struct Process *)FindTask(NULL);
    APTR oldWindow = NULL;
    BOOL isProcess = me->pr_Task.tc_Node.ln_Type == NT_PROCESS;
    UBYTE header[20];
    ULONG indexLen, fileLen, i, pos;
    UWORD n;

    rteEmojiTried = TRUE;
    if (!isProcess)
        return;
    /* no "please insert volume" requesters for missing assigns */
    oldWindow = me->pr_WindowPtr;
    me->pr_WindowPtr = (APTR)-1;
    for (n = 0; rteEmojiPaths[n] != NULL && rteEmojiFile == 0; n++)
        rteEmojiFile = Open((STRPTR)rteEmojiPaths[n], MODE_OLDFILE);
    me->pr_WindowPtr = oldWindow;
    if (rteEmojiFile == 0)
        return;

    if (Read(rteEmojiFile, header, sizeof(header)) != sizeof(header) ||
        header[0] != 'B' || header[1] != 'T' || header[2] != 'E' ||
        header[3] != 'M' || rteGet16(header + 4) != 1)
        goto fail;
    rteEmojiPixelSize = (UWORD)rteGet16(header + 6);
    rteEmojiCount = (UWORD)rteGet16(header + 8);
    rteEmojiColors = (UWORD)rteGet16(header + 10);
    indexLen = rteGet32(header + 12);
    rteEmojiDataOffset = rteGet32(header + 16);
    if (rteEmojiPixelSize == 0 || rteEmojiPixelSize > 64 || rteEmojiCount == 0 ||
        rteEmojiCount > 0x1fff || rteEmojiColors > 255 ||
        rteEmojiDataOffset != 20 + rteEmojiColors * 3UL + indexLen)
        goto fail;

    fileLen = rteFileSize(rteEmojiFile);
    if (fileLen < rteEmojiDataOffset || !rteFileSeek(rteEmojiFile, 0))
        goto fail;
    rteEmojiHeadLen = rteEmojiDataOffset;
    rteEmojiHead = AllocVec(rteEmojiHeadLen, MEMF_ANY);
    rteEmojiRecs = AllocVec(rteEmojiCount * sizeof(UBYTE *), MEMF_ANY);
    rteEmojiOffsets = AllocVec((rteEmojiCount + 1) * sizeof(ULONG), MEMF_ANY);
    rteEmojiPixels =
        AllocVec(rteEmojiCount * sizeof(UBYTE *), MEMF_ANY | MEMF_CLEAR);
    if (rteEmojiHead == NULL || rteEmojiRecs == NULL ||
        rteEmojiOffsets == NULL || rteEmojiPixels == NULL)
        goto fail;
    if (Read(rteEmojiFile, rteEmojiHead, rteEmojiHeadLen) !=
        (LONG)rteEmojiHeadLen)
        goto fail;
    rteEmojiPalette = rteEmojiHead + 20;

    pos = 20 + rteEmojiColors * 3UL;
    rteEmojiMaxLen = 0;
    for (i = 0; i < rteEmojiCount; i++) {
        UBYTE len;
        if (pos >= rteEmojiHeadLen)
            goto fail;
        len = rteEmojiHead[pos] & 0x7f;
        if (len == 0 || pos + 1 + len * 3UL + 3 > rteEmojiHeadLen)
            goto fail;
        rteEmojiRecs[i] = rteEmojiHead + pos;
        rteEmojiOffsets[i] = rteGet24(rteEmojiHead + pos + 1 + len * 3);
        if (len > rteEmojiMaxLen)
            rteEmojiMaxLen = len;
        pos += 1 + len * 3UL + 3;
    }
    rteEmojiOffsets[rteEmojiCount] = fileLen - rteEmojiDataOffset;
    if (rteEmojiMaxLen > RTE_EMOJI_MAXSEQ)
        rteEmojiMaxLen = RTE_EMOJI_MAXSEQ;
    rteEmojiLoaded = TRUE;
    return;

fail:
    rteUnloadEmoji();
}

/**
 * Load the emoji index the first time it is needed. Called from
 * MUIM_Setup, where the caller is the application's process.
 */
BOOL rteEmojiReady(void) {
    if (rteEmojiLoaded)
        return TRUE;
    if (rteEmojiTried)
        return FALSE;
    ObtainSemaphore(&rteEmojiLock);
    if (!rteEmojiTried)
        rteLoadEmoji();
    ReleaseSemaphore(&rteEmojiLock);
    return rteEmojiLoaded;
}

/** Compare an index record with the first n code points of seq */
static LONG rteEmojiCompare(const UBYTE *rec, const ULONG *seq, ULONG n) {
    ULONG len = rec[0] & 0x7f;
    ULONG k, m = len < n ? len : n;
    for (k = 0; k < m; k++) {
        ULONG v = rteGet24(rec + 1 + k * 3);
        if (v != seq[k])
            return v < seq[k] ? -1 : 1;
    }
    return (LONG)len - (LONG)n;
}

/** First record not less than seq[0..n) */
static ULONG rteEmojiLowerBound(const ULONG *seq, ULONG n) {
    ULONG lo = 0, hi = rteEmojiCount;
    while (lo < hi) {
        ULONG mid = (lo + hi) / 2;
        if (rteEmojiCompare(rteEmojiRecs[mid], seq, n) < 0)
            lo = mid + 1;
        else
            hi = mid;
    }
    return lo;
}

/**
 * The longest emoji sequence at the start of seq
 * @return its length in code points (0 when none), with its index
 */
static ULONG rteEmojiMatch(const ULONG *seq, ULONG n, UWORD *index) {
    ULONG k, best = 0;
    if (!rteEmojiLoaded)
        return 0;
    for (k = 1; k <= n; k++) {
        ULONG at = rteEmojiLowerBound(seq, k);
        const UBYTE *rec;
        ULONG len, j;
        if (at >= rteEmojiCount)
            break;
        rec = rteEmojiRecs[at];
        len = rec[0] & 0x7f;
        if (len < k)
            break;
        for (j = 0; j < k; j++) {
            if (rteGet24(rec + 1 + j * 3) != seq[j])
                break;
        }
        if (j < k)
            break; /* no sequence starts like this */
        if (len == k) {
            best = k;
            *index = (UWORD)at;
        }
    }
    return best;
}

/** The decoded pixels of an emoji (size * size, 0 = transparent) */
static const UBYTE *rteEmojiMaster(UWORD index) {
    UBYTE *pixels, *packed;
    ULONG packedLen, total, o = 0, i = 0;

    if (!rteEmojiLoaded || index >= rteEmojiCount)
        return NULL;
    if (rteEmojiPixels[index] != NULL)
        return rteEmojiPixels[index];
    ObtainSemaphore(&rteEmojiLock);
    if (rteEmojiPixels[index] != NULL) {
        ReleaseSemaphore(&rteEmojiLock);
        return rteEmojiPixels[index];
    }
    total = (ULONG)rteEmojiPixelSize * rteEmojiPixelSize;
    packedLen = rteEmojiOffsets[index + 1] - rteEmojiOffsets[index];
    pixels = AllocVec(total, MEMF_ANY | MEMF_CLEAR);
    packed = packedLen > 0 && packedLen < 0x10000
                 ? AllocVec(packedLen, MEMF_ANY)
                 : NULL;
    if (pixels != NULL && packed != NULL &&
        rteFileSeek(rteEmojiFile,
                    rteEmojiDataOffset + rteEmojiOffsets[index]) &&
        Read(rteEmojiFile, packed, packedLen) == (LONG)packedLen) {
        while (o < total && i < packedLen) {
            UBYTE c = packed[i++];
            if (c & 0x80) {
                UBYTE count = c & 0x7f, v = i < packedLen ? packed[i++] : 0;
                while (count-- > 0 && o < total)
                    pixels[o++] = v;
            } else {
                while (c-- > 0 && o < total && i < packedLen)
                    pixels[o++] = packed[i++];
            }
        }
        rteEmojiPixels[index] = pixels;
        pixels = NULL;
    }
    if (packed != NULL)
        FreeVec(packed);
    if (pixels != NULL)
        FreeVec(pixels);
    ReleaseSemaphore(&rteEmojiLock);
    return rteEmojiPixels[index];
}

void rteInitUnicode(void) {
    InitSemaphore(&rteEmojiLock);
    rteInitCodeset();
}

void rteFreeUnicode(void) { rteUnloadEmoji(); }

/* ------------------------------------------------------------------------ */
/* Clusters                                                                  */
/* ------------------------------------------------------------------------ */

static BOOL rteIsKeycapBase(ULONG cp) {
    return cp == '#' || cp == '*' || (cp >= '0' && cp <= '9');
}

/**
 * Split off the cluster at text[off] and say how it is drawn
 * @param glyph receives the RTE_G_* value for the cluster's first byte
 *        (may be NULL)
 * @return its length in bytes (at least 1)
 */
ULONG rteSegment(CONST_STRPTR text, ULONG len, ULONG off, UWORD *glyph) {
    const UBYTE *t = (const UBYTE *)text;
    ULONG cp, n, end;
    UWORD g;
    UWORD emojiIndex = 0;
    BOOL singleEmoji = FALSE;

    if (off >= len) {
        if (glyph != NULL)
            *glyph = RTE_G_GLYPH;
        return 1;
    }
    n = rteDecodeUTF8(text + off, len - off, &cp);

    /* Emoji: a sequence from the emoji set, matched as long as possible */
    if (rteEmojiLoaded &&
        (cp >= 0x203c || cp == 0xa9 || cp == 0xae ||
         (rteIsKeycapBase(cp) && off + 1 < len && t[off + 1] >= 0x80))) {
        ULONG seq[RTE_EMOJI_MAXSEQ];
        ULONG ends[RTE_EMOJI_MAXSEQ];
        ULONG count = 0, pos = off, matched;
        while (pos < len && count < rteEmojiMaxLen) {
            ULONG c, m = rteDecodeUTF8(text + pos, len - pos, &c);
            if (c == 0xfe0f && count > 0) {
                pos += m;
                ends[count - 1] = pos;
                continue;
            }
            if (count > 0 && c < 0x80)
                break;
            seq[count] = c;
            pos += m;
            ends[count++] = pos;
        }
        matched = rteEmojiMatch(seq, count, &emojiIndex);
        if (matched > 0) {
            ULONG after = off + n, c2 = 0;
            BOOL vs16 = FALSE, vs15 = FALSE;
            if (after < len) {
                rteDecodeUTF8(text + after, len - after, &c2);
                vs16 = c2 == 0xfe0f;
                vs15 = c2 == 0xfe0e;
            }
            if (matched > 1 ||
                (!vs15 && (vs16 || (rteEmojiRecs[emojiIndex][0] & 0x80)))) {
                end = ends[matched - 1];
                /* a variation selector after the sequence belongs to it */
                while (end < len) {
                    ULONG c, m = rteDecodeUTF8(text + end, len - end, &c);
                    if (c != 0xfe0f && c != 0xfe0e)
                        break;
                    end += m;
                }
                if (glyph != NULL)
                    *glyph = RTE_G_EMOJI | emojiIndex;
                return end - off;
            }
            /* a text style character with an emoji form: decided below */
            singleEmoji = matched == 1 && !vs15;
        }
    }

    /* Text: the character with the marks that combine with it */
    end = off + n;
    {
        ULONG drawn = cp;
        while (end < len) {
            ULONG mark, m;
            if (t[end] < 0x80)
                break;
            m = rteDecodeUTF8(text + end, len - end, &mark);
            if (!rteIsCombining(mark))
                break;
            if (drawn == cp) {
                ULONG composed = rteCompose(cp, mark);
                if (composed != 0)
                    drawn = composed;
            }
            end += m;
        }
        cp = drawn;
    }
    if (cp < 0x80) {
        g = RTE_G_GLYPH | (UWORD)cp;
    } else {
        LONG sys = rteUniToSys(cp);
        LONG tr;
        if (sys >= 0)
            g = RTE_G_GLYPH | (UWORD)sys;
        else if ((tr = rteFindTranslit(cp)) >= 0)
            g = RTE_G_TRANS | (UWORD)tr;
        else if (singleEmoji)
            g = RTE_G_EMOJI | emojiIndex;
        else if (rteIsCombining(cp))
            g = RTE_G_TRANS; /* a mark with nothing to combine with */
        else
            g = RTE_G_MISSING;
    }
    if (glyph != NULL)
        *glyph = g;
    return end - off;
}

ULONG rteClusterEnd(struct RTEPara *p, ULONG off) {
    if (off >= p->len)
        return p->len;
    if (p->text[off] < 0x80 &&
        (off + 1 >= p->len || p->text[off + 1] < 0x80))
        return off + 1;
    off += rteSegment(p->text, p->len, off, NULL);
    return off > p->len ? p->len : off;
}

/**
 * Start of the cluster that holds off (off itself when a cluster starts
 * there). Every ASCII byte starts a cluster, so the walk starts at the last
 * one before off.
 */
ULONG rteClusterStart(struct RTEPara *p, ULONG off) {
    ULONG j, pos;
    if (off == 0 || off >= p->len)
        return off > p->len ? p->len : off;
    if (p->text[off] < 0x80)
        return off;
    j = off;
    while (j > 0 && p->text[j] >= 0x80)
        j--;
    pos = j;
    while (pos < off) {
        ULONG next = rteClusterEnd(p, pos);
        if (next > off)
            return pos;
        pos = next;
    }
    return off;
}

UWORD rteGlyphAt(struct RTEPara *p, ULONG off) {
    UWORD g;
    if (p->glyphs != NULL && off < p->glyphLen && p->glyphLen == p->len)
        return p->glyphs[off];
    if (off >= p->len)
        return 0;
    if (p->text[off] < 0x80 &&
        (off + 1 >= p->len || p->text[off + 1] < 0x80))
        return p->text[off];
    if (rteClusterStart(p, off) != off)
        return RTE_G_CONT;
    rteSegment(p->text, p->len, off, &g);
    return g;
}

/**
 * Work out how every byte of a paragraph is drawn. Paragraphs of plain
 * ASCII need nothing.
 */
void rteAnnotate(struct RTEData *data, struct RTEPara *p) {
    ULONG i;
    BOOL any = FALSE;

    p->glyphLen = 0;
    for (i = 0; i < p->len; i++) {
        if (p->text[i] >= 0x80) {
            any = TRUE;
            break;
        }
    }
    if (!any) {
        if (p->glyphs != NULL && p->glyphCap > 256) {
            FreePooled(data->pool, p->glyphs, p->glyphCap * sizeof(UWORD));
            p->glyphs = NULL;
            p->glyphCap = 0;
        }
        return;
    }
    if (p->glyphs == NULL || p->glyphCap < p->len) {
        ULONG cap = (p->len + 31) & ~15UL;
        UWORD *g = AllocPooled(data->pool, cap * sizeof(UWORD));
        if (g == NULL)
            return;
        if (p->glyphs != NULL)
            FreePooled(data->pool, p->glyphs, p->glyphCap * sizeof(UWORD));
        p->glyphs = g;
        p->glyphCap = cap;
    }
    i = 0;
    while (i < p->len) {
        ULONG n, k;
        UWORD g;
        if (p->text[i] < 0x80 &&
            (i + 1 >= p->len || p->text[i + 1] < 0x80)) {
            p->glyphs[i] = p->text[i];
            i++;
            continue;
        }
        n = rteSegment(p->text, p->len, i, &g);
        p->glyphs[i] = g;
        for (k = 1; k < n && i + k < p->len; k++)
            p->glyphs[i + k] = RTE_G_CONT;
        i += n;
    }
    p->glyphLen = p->len;
}

/**
 * The system codeset bytes drawn for a glyph
 * @param out room for 8 bytes
 * @return how many (0 for emoji, boxes and invisible characters)
 */
ULONG rteGlyphBytes(UWORD glyph, UBYTE *out) {
    switch (RTE_G_KIND(glyph)) {
    case RTE_G_GLYPH:
        out[0] = (UBYTE)RTE_G_VALUE(glyph);
        return 1;
    case RTE_G_TRANS: {
        const char *s = rteTranslits[RTE_G_VALUE(glyph) % RTE_NUM_TRANSLITS].text;
        ULONG n = 0, len = strlen(s), i = 0;
        while (i < len && n < 8) {
            ULONG cp;
            LONG b;
            i += rteDecodeUTF8(s + i, len - i, &cp);
            b = rteUniToSys(cp);
            out[n++] = b >= 0 ? (UBYTE)b : '?';
        }
        return n;
    }
    }
    return 0;
}

/* ------------------------------------------------------------------------ */
/* Conversions                                                               */
/* ------------------------------------------------------------------------ */

/** System codeset to UTF-8. Returns an AllocVec'd string. */
STRPTR rteToUTF8(CONST_STRPTR s, LONG len, ULONG *outLen) {
    ULONG n = len < 0 ? strlen(s) : (ULONG)len;
    ULONG i, o = 0;
    STRPTR out = AllocVec(rteSystemUTF8 ? n + 1 : n * 3 + 1, MEMF_ANY);
    if (out == NULL)
        return NULL;
    if (rteSystemUTF8) {
        CopyMem((APTR)s, out, n);
        o = n;
    } else {
        for (i = 0; i < n; i++) {
            UBYTE c = s[i];
            if (c < 0x80)
                out[o++] = c;
            else
                o += rteEncodeUTF8(rteSysUni[c], (UBYTE *)out + o);
        }
    }
    out[o] = '\0';
    if (outLen != NULL)
        *outLen = o;
    return out;
}

/**
 * UTF-8 to the system codeset, cluster by cluster as it is drawn: emoji and
 * characters the codeset lacks become '?'. Returns an AllocVec'd string.
 */
STRPTR rteFromUTF8(CONST_STRPTR s, LONG len, ULONG *outLen) {
    ULONG n = len < 0 ? strlen(s) : (ULONG)len;
    ULONG i = 0, o = 0;
    /* no cluster turns into more bytes than it has */
    STRPTR out = AllocVec(n + 1, MEMF_ANY);
    if (out == NULL)
        return NULL;
    if (rteSystemUTF8) {
        CopyMem((APTR)s, out, n);
        o = n;
    } else {
        while (i < n) {
            UBYTE b[8];
            UWORD g;
            ULONG k, m;
            if ((UBYTE)s[i] < 0x80 && (i + 1 >= n || (UBYTE)s[i + 1] < 0x80)) {
                out[o++] = s[i++];
                continue;
            }
            m = rteSegment(s, n, i, &g);
            k = rteGlyphBytes(g, b);
            if (RTE_G_KIND(g) == RTE_G_EMOJI || RTE_G_KIND(g) == RTE_G_MISSING) {
                b[0] = '?';
                k = 1;
            }
            if (k > m)
                k = m;
            CopyMem(b, out + o, k);
            o += k;
            i += m;
        }
    }
    out[o] = '\0';
    if (outLen != NULL)
        *outLen = o;
    return out;
}

STRPTR rteApiToUTF8(struct RTEData *data, CONST_STRPTR s, LONG len,
                    ULONG *outLen) {
    if (data->utf8) {
        ULONG n = len < 0 ? strlen(s) : (ULONG)len;
        STRPTR out = AllocVec(n + 1, MEMF_ANY);
        if (out != NULL) {
            CopyMem((APTR)s, out, n);
            out[n] = '\0';
            if (outLen != NULL)
                *outLen = n;
        }
        return out;
    }
    return rteToUTF8(s, len, outLen);
}

STRPTR rteUTF8ToApi(struct RTEData *data, CONST_STRPTR s, LONG len,
                    ULONG *outLen) {
    if (data->utf8) {
        ULONG n = len < 0 ? strlen(s) : (ULONG)len;
        STRPTR out = AllocVec(n + 1, MEMF_ANY);
        if (out != NULL) {
            CopyMem((APTR)s, out, n);
            out[n] = '\0';
            if (outLen != NULL)
                *outLen = n;
        }
        return out;
    }
    return rteFromUTF8(s, len, outLen);
}

/** Bytes a cluster turns into in the system codeset (see rteFromUTF8) */
static ULONG rteExportWidth(struct RTEPara *p, ULONG off, ULONG *next) {
    UWORD g;
    UBYTE b[8];
    ULONG k;
    if (p->text[off] < 0x80 &&
        (off + 1 >= p->len || p->text[off + 1] < 0x80)) {
        *next = off + 1;
        return 1;
    }
    *next = off + rteSegment(p->text, p->len, off, &g);
    if (RTE_G_KIND(g) == RTE_G_EMOJI || RTE_G_KIND(g) == RTE_G_MISSING)
        return 1;
    k = rteGlyphBytes(g, b);
    return k > *next - off ? *next - off : k;
}

/**
 * The column the application sees for a byte offset: the offset itself in
 * UTF-8 mode, else the offset in the text as exported in the system
 * codeset
 */
ULONG rteOffsetToColumn(struct RTEData *data, struct RTEPara *p, ULONG off) {
    ULONG i = 0, col = 0;
    if (data->utf8 || rteSystemUTF8)
        return off;
    if (off > p->len)
        off = p->len;
    while (i < off) {
        ULONG next;
        col += rteExportWidth(p, i, &next);
        i = next;
    }
    return col;
}

ULONG rteColumnToOffset(struct RTEData *data, struct RTEPara *p, ULONG col) {
    ULONG i = 0, c = 0;
    if (data->utf8 || rteSystemUTF8)
        return rteClusterStart(p, col > p->len ? p->len : col);
    while (i < p->len) {
        ULONG next, w = rteExportWidth(p, i, &next);
        if (c + w > col)
            break;
        c += w;
        i = next;
        if (c == col)
            break;
    }
    return i;
}

ULONG rtePosToApiIndex(struct RTEData *data, struct RTEPos pos) {
    ULONG index = 0, i;
    if (data->utf8 || rteSystemUTF8)
        return rtePosToIndex(data, pos);
    if (pos.para >= data->numParas) {
        pos.para = data->numParas - 1;
        pos.offset = data->paras[pos.para].len;
    }
    for (i = 0; i < pos.para; i++)
        index += rteOffsetToColumn(data, &data->paras[i],
                                   data->paras[i].len) + 1;
    return index + rteOffsetToColumn(data, &data->paras[pos.para], pos.offset);
}

struct RTEPos rteApiIndexToPos(struct RTEData *data, ULONG index) {
    struct RTEPos pos;
    ULONG i;
    if (data->utf8 || rteSystemUTF8) {
        pos = rteIndexToPos(data, index);
        pos.offset = rteClusterStart(&data->paras[pos.para], pos.offset);
        return pos;
    }
    for (i = 0; i < data->numParas; i++) {
        struct RTEPara *p = &data->paras[i];
        ULONG len = rteOffsetToColumn(data, p, p->len);
        if (index <= len) {
            pos.para = i;
            pos.offset = rteColumnToOffset(data, p, index);
            return pos;
        }
        index -= len + 1;
    }
    pos.para = data->numParas - 1;
    pos.offset = data->paras[pos.para].len;
    return pos;
}

/* ------------------------------------------------------------------------ */
/* Emoji bitmaps                                                             */
/* ------------------------------------------------------------------------ */

static LONG rteEmojiPen(struct RTEData *data, Object *obj, UBYTE value) {
    if (data->emojiPens[value] < 0) {
        struct TagItem tags[3];
        const UBYTE *rgb = rteEmojiPalette + (value - 1) * 3;
        LONG pen;
        tags[0].ti_Tag = OBP_Precision;
        tags[0].ti_Data = PRECISION_IMAGE;
        tags[1].ti_Tag = OBP_FailIfBad;
        tags[1].ti_Data = FALSE;
        tags[2].ti_Tag = TAG_DONE;
        pen = ObtainBestPenA(_screen(obj)->ViewPort.ColorMap,
                             rgb[0] * 0x01010101UL, rgb[1] * 0x01010101UL,
                             rgb[2] * 0x01010101UL, tags);
        data->emojiPenAlloc[value] = pen >= 0;
        data->emojiPens[value] = pen >= 0 ? pen : data->pens[RTE_PEN_TEXT];
    }
    return data->emojiPens[value];
}

/**
 * Render an emoji at the given size for the object's screen: the colour
 * pixels go into a bitmap like the screen's, the shape into a mask
 */
static struct RTEEmojiBitMap *rteMakeEmoji(struct RTEData *data, Object *obj,
                                           UWORD index, UWORD size) {
    const UBYTE *master = rteEmojiMaster(index);
    struct BitMap *screenBM = _screen(obj)->RastPort.BitMap;
    struct RTEEmojiBitMap *e;
    UBYTE *rgb = NULL;
    UBYTE *pick = NULL;
    ULONG depth = GetBitMapAttr(screenBM, BMA_DEPTH);
    ULONG rowBytes = ((size + 15) >> 3) & ~1UL;
    BOOL chunky = FALSE, trueColor = FALSE;
    struct RastPort rp;
    UWORD S = rteEmojiPixelSize;
    UWORD x, y;
#if !defined(__AMIGAOS4__)
    struct Library *CyberGfxBase = NULL;
#endif

    if (master == NULL || size == 0)
        return NULL;
    e = AllocVec(sizeof(*e), MEMF_ANY | MEMF_CLEAR);
    if (e == NULL)
        return NULL;
    e->index = index;
    e->size = size;

#if defined(__AMIGAOS4__)
    chunky = TRUE;
    trueColor = depth > 8 && GfxBase->lib_Version >= 54;
#else
    CyberGfxBase = OpenLibrary("cybergraphics.library", 40);
    if (CyberGfxBase != NULL &&
        GetCyberMapAttr(screenBM, CYBRMATTR_ISCYBERGFX)) {
        chunky = TRUE;
        trueColor = depth > 8;
    }
#endif

    e->bitmap = AllocBitMap(size, size, depth, BMF_CLEAR | BMF_MINPLANES,
                            chunky ? screenBM : NULL);
    e->mask = AllocRaster(size, size);
    rgb = AllocVec((ULONG)size * size * 3, MEMF_ANY);
    pick = AllocVec((ULONG)size * size, MEMF_ANY | MEMF_CLEAR);
    if (e->bitmap == NULL || e->mask == NULL || rgb == NULL || pick == NULL)
        goto fail;
    memset(e->mask, 0, rowBytes * size);

    /* Scale by averaging the source pixels each target pixel covers. The
     * shape comes from how much of the area is opaque. */
    for (y = 0; y < size; y++) {
        ULONG sy0 = (ULONG)y * S / size, sy1 = (ULONG)(y + 1) * S / size;
        if (sy1 <= sy0)
            sy1 = sy0 + 1;
        for (x = 0; x < size; x++) {
            ULONG sx0 = (ULONG)x * S / size, sx1 = (ULONG)(x + 1) * S / size;
            ULONG r = 0, g = 0, b = 0, opaque = 0, total = 0, sx, sy;
            UBYTE centre = master[((sy0 + sy1) / 2) * S + (sx0 + sx1) / 2];
            UBYTE any = 0;
            if (sx1 <= sx0)
                sx1 = sx0 + 1;
            for (sy = sy0; sy < sy1; sy++) {
                for (sx = sx0; sx < sx1; sx++) {
                    UBYTE v = master[sy * S + sx];
                    total++;
                    if (v != 0 && v <= rteEmojiColors) {
                        const UBYTE *c = rteEmojiPalette + (v - 1) * 3;
                        r += c[0];
                        g += c[1];
                        b += c[2];
                        opaque++;
                        any = v;
                    }
                }
            }
            if (opaque * 2 >= total && opaque > 0) {
                ULONG o = ((ULONG)y * size + x) * 3;
                rgb[o] = (UBYTE)(r / opaque);
                rgb[o + 1] = (UBYTE)(g / opaque);
                rgb[o + 2] = (UBYTE)(b / opaque);
                pick[y * size + x] = centre != 0 ? centre : any;
                e->mask[y * rowBytes + (x >> 3)] |= (UBYTE)(0x80 >> (x & 7));
            }
        }
    }

    InitRastPort(&rp);
    rp.BitMap = e->bitmap;
    if (trueColor) {
#if defined(__AMIGAOS4__)
        WritePixelArray(rgb, 0, 0, (ULONG)size * 3, PIXF_R8G8B8, &rp, 0, 0,
                        size, size);
#else
        WritePixelArray(rgb, 0, 0, (UWORD)(size * 3), &rp, 0, 0, size, size,
                        RECTFMT_RGB);
#endif
    } else {
        LONG lastPen = -1;
        for (y = 0; y < size; y++) {
            for (x = 0; x < size; x++) {
                UBYTE v = pick[y * size + x];
                LONG pen;
                if (v == 0)
                    continue;
                pen = rteEmojiPen(data, obj, v);
                if (pen != lastPen) {
                    SetAPen(&rp, pen);
                    lastPen = pen;
                }
                WritePixel(&rp, x, y);
            }
        }
    }
    FreeVec(rgb);
    FreeVec(pick);
#if !defined(__AMIGAOS4__)
    if (CyberGfxBase != NULL)
        CloseLibrary(CyberGfxBase);
#endif
    AddTail((struct List *)&data->emoji, (struct Node *)e);
    return e;

fail:
    if (rgb != NULL)
        FreeVec(rgb);
    if (pick != NULL)
        FreeVec(pick);
    if (e->bitmap != NULL)
        FreeBitMap(e->bitmap);
    if (e->mask != NULL)
        FreeRaster(e->mask, size, size);
    FreeVec(e);
#if !defined(__AMIGAOS4__)
    if (CyberGfxBase != NULL)
        CloseLibrary(CyberGfxBase);
#endif
    return NULL;
}

/**
 * Draw an emoji with its top left corner at x, y, clipped to the
 * inclusive rectangle
 * @return FALSE when the emoji could not be drawn
 */
BOOL rteDrawEmoji(struct RTEData *data, Object *obj, struct RastPort *rp,
                  UWORD index, LONG size, LONG x, LONG y, LONG clipL,
                  LONG clipT, LONG clipR, LONG clipB) {
    struct RTEEmojiBitMap *e;
    LONG sx = 0, sy = 0, w = size, h = size;

    if (size <= 0 || size > 255)
        return FALSE;
    for (e = (struct RTEEmojiBitMap *)data->emoji.mlh_Head;
         e->node.mln_Succ != NULL;
         e = (struct RTEEmojiBitMap *)e->node.mln_Succ) {
        if (e->index == index && e->size == size)
            break;
    }
    if (e->node.mln_Succ == NULL) {
        e = rteMakeEmoji(data, obj, index, (UWORD)size);
        if (e == NULL)
            return FALSE;
    }
    if (x < clipL) {
        sx = clipL - x;
        w -= sx;
        x = clipL;
    }
    if (y < clipT) {
        sy = clipT - y;
        h -= sy;
        y = clipT;
    }
    if (x + w - 1 > clipR)
        w = clipR - x + 1;
    if (y + h - 1 > clipB)
        h = clipB - y + 1;
    if (w > 0 && h > 0)
        /* minterm 0xe0 = ABC | ABNC | ANBC: copy the source through the
         * mask */
        BltMaskBitMapRastPort(e->bitmap, sx, sy, rp, x, y, w, h, 0xe0,
                              e->mask);
    return TRUE;
}

void rteFreeEmojiBitMaps(struct RTEData *data, Object *obj) {
    struct RTEEmojiBitMap *e;
    UWORD i;
    BOOL waited = FALSE;
    while ((e = (struct RTEEmojiBitMap *)RemHead(
                (struct List *)&data->emoji)) != NULL) {
        if (!waited) {
            WaitBlit();
            waited = TRUE;
        }
        FreeBitMap(e->bitmap);
        FreeRaster(e->mask, e->size, e->size);
        FreeVec(e);
    }
    for (i = 0; i < 256; i++) {
        if (data->emojiPenAlloc[i] && obj != NULL)
            ReleasePen(_screen(obj)->ViewPort.ColorMap, data->emojiPens[i]);
        data->emojiPenAlloc[i] = FALSE;
        data->emojiPens[i] = -1;
    }
}
