#include <devices/input.h>
#include <devices/inputevent.h>
#include <dos/dos.h>
#include <exec/io.h>
#include <graphics/gfx.h>
#include <graphics/rastport.h>
#include <graphics/view.h>
#include <intuition/intuitionbase.h>
#include <intuition/screens.h>
#include <proto/dos.h>
#include <proto/exec.h>
#include <proto/graphics.h>
#include <proto/intuition.h>
#include <proto/keymap.h>
#if !defined(__AMIGAOS4__)
#include <cybergraphx/cybergraphics.h>
#include <proto/cybergraphics.h>
#endif
#include <stdio.h>
#include <string.h>
#include <strings.h>
#include <zlib.h>
#include "agent_internal.h"

/**
 * Looking at and operating the desktop: a picture of the front screen, and
 * mouse and keyboard input fed to the system as if the user had made it.
 *
 * Screens can be much larger than a model needs or a slow link can carry, so
 * the picture is scaled down. Mouse coordinates are given in the pixels of
 * that picture and converted back here, so the model can point at what it
 * sees without doing arithmetic.
 **/

#define DESKTOP_MAX_IMAGE_WIDTH 1024
#define DESKTOP_MAX_IMAGE_HEIGHT 768
#define DESKTOP_DRAG_STEPS 12

#ifndef IEQUALIFIER_MIDBUTTON
#define IEQUALIFIER_MIDBUTTON 0x1000
#endif

struct NamedKey {
    CONST_STRPTR name;
    UWORD code;
};

static const struct NamedKey NAMED_KEYS[] = {
    {"return", 0x44},    {"enter", 0x43}, {"escape", 0x45}, {"esc", 0x45},
    {"backspace", 0x41}, {"tab", 0x42},   {"delete", 0x46}, {"del", 0x46},
    {"space", 0x40},     {"up", 0x4C},    {"down", 0x4D},   {"right", 0x4E},
    {"left", 0x4F},      {"help", 0x5F},  {"f1", 0x50},     {"f2", 0x51},
    {"f3", 0x52},        {"f4", 0x53},    {"f5", 0x54},     {"f6", 0x55},
    {"f7", 0x56},        {"f8", 0x57},    {"f9", 0x58},     {"f10", 0x59},
    {NULL, 0}};

/* The front screen and the scale between it and the picture of it, as a
 * 16.16 fixed point number of screen pixels per picture pixel */
struct DesktopGeometry {
    struct Screen *screen;
    ULONG screenWidth;
    ULONG screenHeight;
    ULONG imageWidth;
    ULONG imageHeight;
    ULONG scale;
};

static BOOL frontScreenGeometry(struct DesktopGeometry *geometry) {
    ULONG lock = LockIBase(0);
    struct Screen *screen = ((struct IntuitionBase *)IntuitionBase)->FirstScreen;
    ULONG scaleX;
    ULONG scaleY;

    memset(geometry, 0, sizeof(*geometry));
    if (screen != NULL) {
        geometry->screen = screen;
        geometry->screenWidth = screen->Width;
        geometry->screenHeight = screen->Height;
    }
    UnlockIBase(lock);
    if (geometry->screen == NULL || geometry->screenWidth == 0 ||
        geometry->screenHeight == 0)
        return FALSE;

    scaleX = (geometry->screenWidth << 16) / DESKTOP_MAX_IMAGE_WIDTH;
    scaleY = (geometry->screenHeight << 16) / DESKTOP_MAX_IMAGE_HEIGHT;
    geometry->scale = scaleX > scaleY ? scaleX : scaleY;
    if (geometry->scale < 0x10000)
        geometry->scale = 0x10000;
    geometry->imageWidth =
        (ULONG)(((unsigned long long)geometry->screenWidth << 16) /
                geometry->scale);
    geometry->imageHeight =
        (ULONG)(((unsigned long long)geometry->screenHeight << 16) /
                geometry->scale);
    if (geometry->imageWidth == 0)
        geometry->imageWidth = 1;
    if (geometry->imageHeight == 0)
        geometry->imageHeight = 1;
    return TRUE;
}

/* ------------------------------------------------------------------------
 * Screenshot
 * ---------------------------------------------------------------------- */

static void putBigEndian32(UBYTE *out, ULONG value) {
    out[0] = (UBYTE)(value >> 24);
    out[1] = (UBYTE)(value >> 16);
    out[2] = (UBYTE)(value >> 8);
    out[3] = (UBYTE)value;
}

/* Append one PNG chunk and return the new write position */
static UBYTE *writePngChunk(UBYTE *out, CONST_STRPTR type, const UBYTE *data,
                            ULONG length) {
    ULONG crc;
    putBigEndian32(out, length);
    CopyMem((APTR)type, out + 4, 4);
    if (length > 0)
        CopyMem((APTR)data, out + 8, length);
    crc = crc32(0L, out + 4, length + 4);
    putBigEndian32(out + 8 + length, crc);
    return out + 12 + length;
}

/**
 * Turn rows of RGB pixels, each row led by a zero filter byte, into a PNG
 * @return the file contents. Free them with FreeVec()
 **/
static UBYTE *encodePng(const UBYTE *scanlines, ULONG width, ULONG height,
                        ULONG *lengthOut) {
    ULONG rawLength = height * (width * 3 + 1);
    uLongf compressedLength = compressBound(rawLength);
    UBYTE *compressed = AllocVec(compressedLength, MEMF_ANY);
    UBYTE *png;
    UBYTE *out;
    UBYTE header[13];
    static const UBYTE signature[8] = {0x89, 'P', 'N', 'G', 13, 10, 26, 10};

    *lengthOut = 0;
    if (compressed == NULL)
        return NULL;
    if (compress2(compressed, &compressedLength, scanlines, rawLength, 6) !=
        Z_OK) {
        FreeVec(compressed);
        return NULL;
    }
    png = AllocVec(compressedLength + 64, MEMF_ANY);
    if (png == NULL) {
        FreeVec(compressed);
        return NULL;
    }
    CopyMem((APTR)signature, png, 8);
    putBigEndian32(header, width);
    putBigEndian32(header + 4, height);
    header[8] = 8;  /* bits per channel */
    header[9] = 2;  /* RGB */
    header[10] = 0; /* deflate */
    header[11] = 0; /* standard filters */
    header[12] = 0; /* not interlaced */
    out = writePngChunk(png + 8, "IHDR", header, 13);
    out = writePngChunk(out, "IDAT", compressed, (ULONG)compressedLength);
    out = writePngChunk(out, "IEND", NULL, 0);
    FreeVec(compressed);
    *lengthOut = (ULONG)(out - png);
    return png;
}

static STRPTR encodeBase64(const UBYTE *data, ULONG length) {
    static CONST_STRPTR alphabet =
        "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    STRPTR out = AllocVec((length + 2) / 3 * 4 + 1, MEMF_ANY);
    ULONG i;
    ULONG o = 0;
    if (out == NULL)
        return NULL;
    for (i = 0; i + 2 < length; i += 3) {
        ULONG triple = ((ULONG)data[i] << 16) | ((ULONG)data[i + 1] << 8) |
                       data[i + 2];
        out[o++] = alphabet[(triple >> 18) & 63];
        out[o++] = alphabet[(triple >> 12) & 63];
        out[o++] = alphabet[(triple >> 6) & 63];
        out[o++] = alphabet[triple & 63];
    }
    if (i < length) {
        ULONG triple = (ULONG)data[i] << 16;
        if (i + 1 < length)
            triple |= (ULONG)data[i + 1] << 8;
        out[o++] = alphabet[(triple >> 18) & 63];
        out[o++] = alphabet[(triple >> 12) & 63];
        out[o++] = i + 1 < length ? alphabet[(triple >> 6) & 63] : '=';
        out[o++] = '=';
    }
    out[o] = '\0';
    return out;
}

/**
 * Read one row of the screen as RGB bytes
 * @param cyber whether the screen can be read through cybergraphics
 * @param palette the screen's colours as RGB bytes, for screens that are
 * read pen by pen
 **/
static void readScreenRow(struct Screen *screen, ULONG y, ULONG width,
                          UBYTE *row, BOOL cyber, const UBYTE *palette
#if !defined(__AMIGAOS4__)
                          ,
                          struct Library *CyberGfxBase
#endif
) {
    ULONG x;
#if defined(__AMIGAOS4__)
    (void)cyber;
    (void)palette;
    for (x = 0; x < width; x++) {
        ULONG colour = ReadPixelColor(&screen->RastPort, x, y);
        row[x * 3] = (UBYTE)(colour >> 16);
        row[x * 3 + 1] = (UBYTE)(colour >> 8);
        row[x * 3 + 2] = (UBYTE)colour;
    }
#else
    if (cyber) {
        ReadPixelArray(row, 0, 0, width * 3, &screen->RastPort, 0, y, width, 1,
                       RECTFMT_RGB);
        return;
    }
    for (x = 0; x < width; x++) {
        ULONG pen = ReadPixel(&screen->RastPort, x, y) & 0xFF;
        row[x * 3] = palette[pen * 3];
        row[x * 3 + 1] = palette[pen * 3 + 1];
        row[x * 3 + 2] = palette[pen * 3 + 2];
    }
#endif
}

BOOL agentDesktopScreenshot(STRPTR *base64Out, STRPTR description,
                            ULONG descriptionSize, CONST_STRPTR savePath) {
    struct DesktopGeometry geometry;
    UBYTE *row = NULL;
    UBYTE *scanlines = NULL;
    ULONG *sums = NULL;
    UBYTE *png = NULL;
    UBYTE palette[256 * 3];
    ULONG pngLength = 0;
    ULONG outY;
    BOOL cyber = FALSE;
    BOOL ok = FALSE;
#if !defined(__AMIGAOS4__)
    struct Library *CyberGfxBase = OpenLibrary("cybergraphics.library", 40);
#endif

    *base64Out = NULL;
    if (description != NULL && descriptionSize > 0)
        description[0] = '\0';
    if (!frontScreenGeometry(&geometry)) {
#if !defined(__AMIGAOS4__)
        if (CyberGfxBase != NULL)
            CloseLibrary(CyberGfxBase);
#endif
        return FALSE;
    }

#if !defined(__AMIGAOS4__)
    if (CyberGfxBase != NULL &&
        GetCyberMapAttr(geometry.screen->RastPort.BitMap,
                        CYBRMATTR_ISCYBERGFX))
        cyber = TRUE;
    if (!cyber) {
        /* A planar screen is read pen by pen, so its colours are needed */
        struct ColorMap *colorMap = geometry.screen->ViewPort.ColorMap;
        ULONG depth =
            GetBitMapAttr(geometry.screen->RastPort.BitMap, BMA_DEPTH);
        ULONG colours = depth >= 8 ? 256 : (1UL << depth);
        ULONG table[3];
        ULONG pen;
        memset(palette, 0, sizeof(palette));
        for (pen = 0; pen < colours && colorMap != NULL; pen++) {
            GetRGB32(colorMap, pen, 1, table);
            palette[pen * 3] = (UBYTE)(table[0] >> 24);
            palette[pen * 3 + 1] = (UBYTE)(table[1] >> 24);
            palette[pen * 3 + 2] = (UBYTE)(table[2] >> 24);
        }
    }
#endif

    row = AllocVec(geometry.screenWidth * 3, MEMF_ANY);
    sums = AllocVec(geometry.imageWidth * 3 * sizeof(ULONG), MEMF_ANY);
    scanlines =
        AllocVec(geometry.imageHeight * (geometry.imageWidth * 3 + 1), MEMF_ANY);
    if (row != NULL && sums != NULL && scanlines != NULL) {
        /* Each picture pixel is the average of the screen pixels it covers,
         * which keeps small text readable where skipping pixels would not */
        for (outY = 0; outY < geometry.imageHeight; outY++) {
            ULONG y0 = (ULONG)(((unsigned long long)outY * geometry.scale) >> 16);
            ULONG y1 =
                (ULONG)(((unsigned long long)(outY + 1) * geometry.scale) >> 16);
            UBYTE *line =
                scanlines + outY * (geometry.imageWidth * 3 + 1);
            ULONG rows = 0;
            ULONG y;
            ULONG outX;

            if (y1 <= y0)
                y1 = y0 + 1;
            if (y1 > geometry.screenHeight)
                y1 = geometry.screenHeight;
            memset(sums, 0, geometry.imageWidth * 3 * sizeof(ULONG));
            for (y = y0; y < y1; y++) {
                readScreenRow(geometry.screen, y, geometry.screenWidth, row,
                              cyber, palette
#if !defined(__AMIGAOS4__)
                              ,
                              CyberGfxBase
#endif
                );
                for (outX = 0; outX < geometry.imageWidth; outX++) {
                    ULONG x0 = (ULONG)(
                        ((unsigned long long)outX * geometry.scale) >> 16);
                    ULONG x1 = (ULONG)(
                        ((unsigned long long)(outX + 1) * geometry.scale) >> 16);
                    ULONG x;
                    if (x1 <= x0)
                        x1 = x0 + 1;
                    if (x1 > geometry.screenWidth)
                        x1 = geometry.screenWidth;
                    for (x = x0; x < x1; x++) {
                        sums[outX * 3] += row[x * 3];
                        sums[outX * 3 + 1] += row[x * 3 + 1];
                        sums[outX * 3 + 2] += row[x * 3 + 2];
                    }
                }
                rows++;
            }
            line[0] = 0;
            for (outX = 0; outX < geometry.imageWidth; outX++) {
                ULONG x0 =
                    (ULONG)(((unsigned long long)outX * geometry.scale) >> 16);
                ULONG x1 = (ULONG)(
                    ((unsigned long long)(outX + 1) * geometry.scale) >> 16);
                ULONG count;
                if (x1 <= x0)
                    x1 = x0 + 1;
                if (x1 > geometry.screenWidth)
                    x1 = geometry.screenWidth;
                count = (x1 - x0) * (rows > 0 ? rows : 1);
                if (count == 0)
                    count = 1;
                line[1 + outX * 3] = (UBYTE)(sums[outX * 3] / count);
                line[2 + outX * 3] = (UBYTE)(sums[outX * 3 + 1] / count);
                line[3 + outX * 3] = (UBYTE)(sums[outX * 3 + 2] / count);
            }
        }
        png = encodePng(scanlines, geometry.imageWidth, geometry.imageHeight,
                        &pngLength);
    }

    if (png != NULL) {
        *base64Out = encodeBase64(png, pngLength);
        if (savePath != NULL && savePath[0] != '\0')
            agentWriteFile(savePath, (CONST_STRPTR)png, pngLength);
        ok = *base64Out != NULL;
        if (ok && description != NULL)
            snprintf(description, descriptionSize,
                     "This is the front screen. The screen is %lu x %lu "
                     "pixels and the picture is %lu x %lu. Give the mouse "
                     "tool coordinates in the pixels of this picture, "
                     "counted from the top left corner.",
                     geometry.screenWidth, geometry.screenHeight,
                     geometry.imageWidth, geometry.imageHeight);
        FreeVec(png);
    }
    if (row != NULL)
        FreeVec(row);
    if (sums != NULL)
        FreeVec(sums);
    if (scanlines != NULL)
        FreeVec(scanlines);
#if !defined(__AMIGAOS4__)
    if (CyberGfxBase != NULL)
        CloseLibrary(CyberGfxBase);
#endif
    return ok;
}

/* ------------------------------------------------------------------------
 * Input
 * ---------------------------------------------------------------------- */

struct InputDevice {
    struct MsgPort *port;
    struct IOStdReq *request;
    BOOL open;
};

static BOOL openInputDevice(struct InputDevice *device) {
    memset(device, 0, sizeof(*device));
    device->port = CreateMsgPort();
    if (device->port == NULL)
        return FALSE;
    device->request = (struct IOStdReq *)CreateIORequest(
        device->port, sizeof(struct IOStdReq));
    if (device->request == NULL)
        return FALSE;
    if (OpenDevice("input.device", 0, (struct IORequest *)device->request, 0) !=
        0)
        return FALSE;
    device->open = TRUE;
    return TRUE;
}

static void closeInputDevice(struct InputDevice *device) {
    if (device->open)
        CloseDevice((struct IORequest *)device->request);
    if (device->request != NULL)
        DeleteIORequest((struct IORequest *)device->request);
    if (device->port != NULL)
        DeleteMsgPort(device->port);
    memset(device, 0, sizeof(*device));
}

/* Hand one event to the system as if it came from the hardware */
static void writeEvent(struct InputDevice *device, struct InputEvent *event) {
    ULONG seconds = 0;
    ULONG micros = 0;
    ULONG *stamp = (ULONG *)&event->ie_TimeStamp;
    /* Programs tell a double click from two clicks by these times */
    CurrentTime(&seconds, &micros);
    stamp[0] = seconds;
    stamp[1] = micros;
    device->request->io_Command = IND_WRITEEVENT;
    device->request->io_Data = (APTR)event;
    device->request->io_Length = sizeof(struct InputEvent);
    DoIO((struct IORequest *)device->request);
}

static void movePointer(struct InputDevice *device, struct Screen *screen,
                        LONG x, LONG y, UWORD qualifier) {
    struct InputEvent event;
    struct IEPointerPixel pixel;
    memset(&event, 0, sizeof(event));
    pixel.iepp_Screen = screen;
    pixel.iepp_Position.X = (WORD)x;
    pixel.iepp_Position.Y = (WORD)y;
    event.ie_Class = IECLASS_NEWPOINTERPOS;
    event.ie_SubClass = IESUBCLASS_PIXEL;
    event.ie_Code = IECODE_NOBUTTON;
    event.ie_Qualifier = qualifier;
    event.ie_EventAddress = (APTR)&pixel;
    writeEvent(device, &event);
}

static void mouseButton(struct InputDevice *device, UWORD code, BOOL down,
                        UWORD heldQualifier) {
    struct InputEvent event;
    memset(&event, 0, sizeof(event));
    event.ie_Class = IECLASS_RAWMOUSE;
    event.ie_Code = down ? code : (code | IECODE_UP_PREFIX);
    event.ie_Qualifier =
        IEQUALIFIER_RELATIVEMOUSE | (down ? heldQualifier : 0);
    writeEvent(device, &event);
}

BOOL agentDesktopMouse(CONST_STRPTR action, CONST_STRPTR button, LONG x,
                       LONG y, LONG toX, LONG toY, STRPTR error,
                       ULONG errorSize) {
    struct DesktopGeometry geometry;
    struct InputDevice device;
    UWORD code = IECODE_LBUTTON;
    UWORD held = IEQUALIFIER_LEFTBUTTON;
    LONG screenX;
    LONG screenY;
    LONG screenToX;
    LONG screenToY;
    BOOL ok = TRUE;

    if (error != NULL && errorSize > 0)
        error[0] = '\0';
    if (!frontScreenGeometry(&geometry)) {
        snprintf(error, errorSize, "There is no screen to work on.");
        return FALSE;
    }
    if (x < 0 || y < 0 || (ULONG)x >= geometry.imageWidth ||
        (ULONG)y >= geometry.imageHeight) {
        snprintf(error, errorSize,
                 "The position is outside the picture, which is %lu x %lu.",
                 geometry.imageWidth, geometry.imageHeight);
        return FALSE;
    }
    if (button != NULL && strcasecmp(button, "right") == 0) {
        code = IECODE_RBUTTON;
        held = IEQUALIFIER_RBUTTON;
    } else if (button != NULL && strcasecmp(button, "middle") == 0) {
        code = IECODE_MBUTTON;
        held = IEQUALIFIER_MIDBUTTON;
    }
    /* Aim at the middle of the screen pixels a picture pixel stands for */
    screenX = (LONG)((((unsigned long long)x * geometry.scale) +
                      geometry.scale / 2) >>
                     16);
    screenY = (LONG)((((unsigned long long)y * geometry.scale) +
                      geometry.scale / 2) >>
                     16);
    if (toX < 0)
        toX = 0;
    if (toY < 0)
        toY = 0;
    if ((ULONG)toX >= geometry.imageWidth)
        toX = geometry.imageWidth - 1;
    if ((ULONG)toY >= geometry.imageHeight)
        toY = geometry.imageHeight - 1;
    screenToX = (LONG)((((unsigned long long)toX * geometry.scale) +
                        geometry.scale / 2) >>
                       16);
    screenToY = (LONG)((((unsigned long long)toY * geometry.scale) +
                        geometry.scale / 2) >>
                       16);

    if (!openInputDevice(&device)) {
        closeInputDevice(&device);
        snprintf(error, errorSize, "The input device could not be opened.");
        return FALSE;
    }

    movePointer(&device, geometry.screen, screenX, screenY, 0);
    Delay(3);
    if (strcasecmp(action, "move") == 0) {
        /* Nothing more to do */
    } else if (strcasecmp(action, "click") == 0) {
        mouseButton(&device, code, TRUE, held);
        Delay(3);
        mouseButton(&device, code, FALSE, held);
    } else if (strcasecmp(action, "double_click") == 0) {
        mouseButton(&device, code, TRUE, held);
        Delay(2);
        mouseButton(&device, code, FALSE, held);
        Delay(4);
        mouseButton(&device, code, TRUE, held);
        Delay(2);
        mouseButton(&device, code, FALSE, held);
    } else if (strcasecmp(action, "drag") == 0) {
        LONG step;
        mouseButton(&device, code, TRUE, held);
        Delay(8);
        /* Programs follow a drag by the pointer moving, so it is moved in
         * steps instead of jumping to the end */
        for (step = 1; step <= DESKTOP_DRAG_STEPS; step++) {
            movePointer(
                &device, geometry.screen,
                screenX + (screenToX - screenX) * step / DESKTOP_DRAG_STEPS,
                screenY + (screenToY - screenY) * step / DESKTOP_DRAG_STEPS,
                held);
            Delay(2);
        }
        Delay(8);
        mouseButton(&device, code, FALSE, held);
    } else {
        snprintf(error, errorSize,
                 "The action must be move, click, double_click or drag.");
        ok = FALSE;
    }
    closeInputDevice(&device);
    /* Give the program under the pointer a moment to react */
    Delay(15);
    return ok;
}

static void pressKey(struct InputDevice *device, UWORD code, UWORD qualifier) {
    struct InputEvent event;
    memset(&event, 0, sizeof(event));
    event.ie_Class = IECLASS_RAWKEY;
    event.ie_Code = code;
    event.ie_Qualifier = qualifier;
    writeEvent(device, &event);
    Delay(1);
    event.ie_Code = code | IECODE_UP_PREFIX;
    writeEvent(device, &event);
    Delay(1);
}

BOOL agentDesktopKeyboard(CONST_STRPTR text, CONST_STRPTR key,
                          UWORD qualifiers, LONG repeat, STRPTR error,
                          ULONG errorSize) {
    struct InputDevice device;
    struct Library *KeymapBase = OpenLibrary("keymap.library", 36);
#if defined(__AMIGAOS4__)
    struct KeymapIFace *IKeymap =
        KeymapBase != NULL
            ? (struct KeymapIFace *)GetInterface(KeymapBase, "main", 1, NULL)
            : NULL;
#endif
    ULONG unmapped = 0;
    BOOL ok = TRUE;
    LONG i;

    if (error != NULL && errorSize > 0)
        error[0] = '\0';
    if (repeat < 1)
        repeat = 1;
    if (repeat > 50)
        repeat = 50;
    if (KeymapBase == NULL
#if defined(__AMIGAOS4__)
        || IKeymap == NULL
#endif
        || !openInputDevice(&device)) {
        if (KeymapBase != NULL) {
#if defined(__AMIGAOS4__)
            if (IKeymap != NULL)
                DropInterface((struct Interface *)IKeymap);
#endif
            CloseLibrary(KeymapBase);
            closeInputDevice(&device);
        }
        snprintf(error, errorSize, "The keyboard could not be reached.");
        return FALSE;
    }

    if (key != NULL && key[0] != '\0') {
        UWORD code = 0xFFFF;
        UWORD keyQualifier = 0;
        for (i = 0; NAMED_KEYS[i].name != NULL; i++) {
            if (strcasecmp(NAMED_KEYS[i].name, key) == 0)
                code = NAMED_KEYS[i].code;
        }
        if (code == 0xFFFF && strlen(key) == 1) {
            /* A character key, such as q for a menu shortcut */
            UBYTE mapped[8];
            if (MapANSI((STRPTR)key, 1, mapped, 4, NULL) >= 1) {
                code = mapped[0];
                keyQualifier = mapped[1];
            }
        }
        if (code == 0xFFFF) {
            snprintf(error, errorSize,
                     "There is no key called \"%s\". Use a single character "
                     "or one of return, enter, escape, backspace, tab, "
                     "delete, space, up, down, left, right, help, f1 to f10.",
                     key);
            ok = FALSE;
        } else {
            for (i = 0; i < repeat; i++)
                pressKey(&device, code, (UWORD)(qualifiers | keyQualifier));
        }
    }

    if (ok && text != NULL) {
        CONST_STRPTR p;
        for (p = text; *p != '\0'; p++) {
            UBYTE mapped[8];
            LONG pairs;
            if (*p == '\r')
                continue;
            if (*p == '\n') {
                pressKey(&device, 0x44, 0);
                continue;
            }
            if (*p == '\t') {
                pressKey(&device, 0x42, 0);
                continue;
            }
            /* The keymap says which key and qualifiers make a character,
             * which may take two key presses for an accented letter */
            pairs = MapANSI((STRPTR)p, 1, mapped, 4, NULL);
            if (pairs < 1) {
                unmapped++;
                continue;
            }
            for (i = 0; i < pairs; i++)
                pressKey(&device, mapped[i * 2], mapped[i * 2 + 1]);
        }
        if (unmapped > 0)
            snprintf(error, errorSize,
                     "%lu characters could not be typed with this keymap and "
                     "were left out.",
                     unmapped);
    }

    closeInputDevice(&device);
#if defined(__AMIGAOS4__)
    DropInterface((struct Interface *)IKeymap);
#endif
    CloseLibrary(KeymapBase);
    Delay(10);
    return ok;
}
