#include <clib/alib_protos.h>
#include <dos/dos.h>
#include <graphics/gfx.h>
#include <graphics/gfxmacros.h>
#include <graphics/rastport.h>
#include <intuition/intuition.h>
#include <intuition/screens.h>
#include <libraries/mui.h>
#include <proto/dos.h>
#include <proto/exec.h>
#include <proto/graphics.h>
#include <proto/intuition.h>
#include <proto/muimaster.h>
#include <proto/utility.h>
#include <SDI_hook.h>
#include <string.h>
#include "gui.h"
#include "speech.h"
#include "SpeechWaveform.h"

#define WAVE_COLS 256
#define WAVE_BANDS 8
#define FFT_N 32
#define CTRL_BAR 20
#define BTN_W 22
#define BTN_H 16
#define BTN_GAP 3
#define BTN_PAD 4
#define BTN_NONE 0
#define BTN_PLAYPAUSE 1
#define BTN_STOP 2
#define BTN_REWIND 3

struct MUI_CustomClass *speechWaveformClass;

struct SpeechWaveformData {
    struct MUI_EventHandlerNode eh;
    BOOL eventHandlerAdded;
    BOOL dragging;
    BOOL hasAudio;
    UBYTE fileName[256];
    UBYTE peaks[WAVE_COLS];
    UBYTE bands[WAVE_COLS][WAVE_BANDS];
    ULONG durationMs;
    ULONG positionMs;
    struct BitMap *cacheBm;
    struct RastPort cacheRp;
    LONG cacheWidth;
    LONG cacheHeight;
    BOOL cacheValid;
    BOOL playing;
    BOOL paused;
    BOOL hasSpeech;
    UBYTE pressedBtn;
    ULONG command;
};

static const WORD COS_TAB[FFT_N] = {
    256,  251,  237,  213,  181,  142,   98,   50,    0,  -50,  -98, -142,
    -181, -213, -237, -251, -256, -251, -237, -213, -181, -142,  -98,  -50,
    0,    50,    98,  142,  181,  213,  237,  251};
static const WORD SIN_TAB[FFT_N] = {
    0,    50,    98,  142,  181,  213,  237,  251,  256,  251,  237,  213,
    181,  142,   98,   50,    0,  -50,  -98, -142, -181, -213, -237, -251,
    -256, -251, -237, -213, -181, -142,  -98,  -50};

static UWORD readLE16(const UBYTE *source) {
    return (UWORD)((UWORD)source[0] | ((UWORD)source[1] << 8));
}

static ULONG readLE32(const UBYTE *source) {
    return (ULONG)source[0] | ((ULONG)source[1] << 8) |
           ((ULONG)source[2] << 16) | ((ULONG)source[3] << 24);
}

static APTR readWholeFile(CONST_STRPTR filename, ULONG *size) {
    BPTR file;
    LONG length;
    UBYTE *buffer;

    *size = 0;
    file = Open(filename, MODE_OLDFILE);
    if (file == 0)
        return NULL;
#ifdef __AMIGAOS3__
    Seek(file, 0, OFFSET_END);
    length = Seek(file, 0, OFFSET_BEGINNING);
#elif defined(__AMIGAOS4__)
    length = (LONG)GetFileSize(file);
#else
    {
        struct FileInfoBlock fib;
        ExamineFH64(file, &fib, NULL);
        length = (LONG)fib.fib_Size;
    }
#endif
    if (length <= 0) {
        Close(file);
        return NULL;
    }
    buffer = AllocVec(length, MEMF_ANY);
    if (buffer == NULL || Read(file, buffer, length) != length) {
        if (buffer != NULL)
            FreeVec(buffer);
        Close(file);
        return NULL;
    }
    Close(file);
    *size = (ULONG)length;
    return buffer;
}

static WORD sampleAt(const UBYTE *data, ULONG index, UWORD bits,
                     UWORD channels, BOOL signedPcm, BOOL bigEndian) {
    ULONG frame;
    LONG left;
    LONG right;

    if (bits == 8) {
        frame = index * channels;
        if (signedPcm)
            left = ((BYTE)data[frame]) << 8;
        else
            left = ((LONG)data[frame] - 128) << 8;
        if (channels > 1) {
            if (signedPcm)
                right = ((BYTE)data[frame + 1]) << 8;
            else
                right = ((LONG)data[frame + 1] - 128) << 8;
        } else
            right = left;
    } else if (bits == 24) {
        frame = index * channels * 3;
        left = (WORD)((data[frame + 2] << 8) | data[frame + 1]);
        if (channels > 1)
            right = (WORD)((data[frame + 5] << 8) | data[frame + 4]);
        else
            right = left;
    } else {
        frame = index * channels * 2;
        if (bigEndian)
            left = (WORD)(((UWORD)data[frame] << 8) | data[frame + 1]);
        else
            left = (WORD)readLE16(data + frame);
        if (channels > 1) {
            if (bigEndian)
                right = (WORD)(((UWORD)data[frame + 2] << 8) | data[frame + 3]);
            else
                right = (WORD)readLE16(data + frame + 2);
        } else
            right = left;
    }
    return (WORD)((left + right) / 2);
}

static void analyzeColumn(const WORD *window, UBYTE *peak, UBYTE *bands) {
    ULONG n;
    ULONG k;
    ULONG maxAbs = 0;

    for (n = 0; n < FFT_N; n++) {
        LONG v = window[n];
        if (v < 0)
            v = -v;
        if ((ULONG)v > maxAbs)
            maxAbs = (ULONG)v;
    }
    *peak = (UBYTE)(maxAbs > 32767 ? 255 : (maxAbs * 255) / 32767);

    for (k = 0; k < WAVE_BANDS; k++) {
        LONG re = 0;
        LONG im = 0;
        ULONG bin = k + 1;
        ULONG mag;
        for (n = 0; n < FFT_N; n++) {
            ULONG idx = (bin * n) & (FFT_N - 1);
            re += ((LONG)window[n] * COS_TAB[idx]) >> 8;
            im += ((LONG)window[n] * SIN_TAB[idx]) >> 8;
        }
        if (re < 0)
            re = -re;
        if (im < 0)
            im = -im;
        mag = (ULONG)(re + im);
        if (mag > 32767)
            mag = 32767;
        bands[k] = (UBYTE)((mag * 255) / 32767);
    }
}

static void freeWaveformCache(struct SpeechWaveformData *data) {
    if (data->cacheBm != NULL) {
        WaitBlit();
        FreeBitMap(data->cacheBm);
        data->cacheBm = NULL;
    }
    data->cacheValid = FALSE;
    data->cacheWidth = 0;
    data->cacheHeight = 0;
}

static void clearWaveform(struct SpeechWaveformData *data) {
    memset(data->peaks, 0, sizeof(data->peaks));
    memset(data->bands, 0, sizeof(data->bands));
    data->hasAudio = FALSE;
    data->durationMs = 0;
    data->positionMs = 0;
    data->fileName[0] = '\0';
    data->cacheValid = FALSE;
}

static BOOL loadWaveformFile(struct SpeechWaveformData *data,
                             CONST_STRPTR filename) {
    ULONG wavLength = 0;
    UBYTE *wav;
    ULONG riff = 0;
    ULONG offset;
    ULONG sampleRate = 0;
    ULONG dataLength = 0;
    UBYTE *pcm = NULL;
    UWORD encoding = 0;
    UWORD channels = 1;
    UWORD bits = 16;
    ULONG frames;
    ULONG col;
    BOOL parsed = FALSE;

    clearWaveform(data);
    if (filename == NULL || filename[0] == '\0')
        return FALSE;

    wav = readWholeFile(filename, &wavLength);
    if (wav == NULL)
        return FALSE;

    while (riff + 12 <= wavLength &&
           (memcmp(wav + riff, "RIFF", 4) != 0 ||
            memcmp(wav + riff + 8, "WAVE", 4) != 0))
        riff++;
    if (riff + 12 <= wavLength) {
        offset = riff + 12;
        while (offset + 8 <= wavLength) {
            ULONG chunkLength = readLE32(wav + offset + 4);
            ULONG chunkData = offset + 8;
            if (chunkData > wavLength)
                break;
            if (chunkLength > wavLength - chunkData)
                chunkLength = wavLength - chunkData;
            if (memcmp(wav + offset, "fmt ", 4) == 0 && chunkLength >= 16) {
                encoding = readLE16(wav + chunkData);
                channels = readLE16(wav + chunkData + 2);
                sampleRate = readLE32(wav + chunkData + 4);
                bits = readLE16(wav + chunkData + 14);
            } else if (memcmp(wav + offset, "data", 4) == 0) {
                pcm = wav + chunkData;
                dataLength = chunkLength;
            }
            offset = chunkData + chunkLength + (chunkLength & 1);
        }
        if (encoding == 0xFFFE)
            encoding = 1;
        parsed = (encoding == 1 && (channels == 1 || channels == 2) &&
                  sampleRate != 0 && pcm != NULL && dataLength != 0 &&
                  (bits == 8 || bits == 16 || bits == 24));
    }
    if (!parsed) {
        pcm = wav;
        dataLength = wavLength;
        sampleRate = 24000;
        channels = 1;
        bits = 16;
    }

    frames = dataLength / (((bits + 7) / 8) * channels);
    if (frames == 0) {
        FreeVec(wav);
        return FALSE;
    }
    data->durationMs = (ULONG)(((unsigned long long)frames * 1000ULL) /
                               (sampleRate ? sampleRate : 24000));

    for (col = 0; col < WAVE_COLS; col++) {
        WORD window[FFT_N];
        ULONG start = (col * frames) / WAVE_COLS;
        ULONG end = ((col + 1) * frames) / WAVE_COLS;
        ULONG n;
        if (end <= start)
            end = start + 1;
        for (n = 0; n < FFT_N; n++) {
            ULONG idx = start + ((end - start) * n) / FFT_N;
            if (idx >= frames)
                idx = frames - 1;
            window[n] = sampleAt(pcm, idx, bits, channels, FALSE, FALSE);
        }
        analyzeColumn(window, &data->peaks[col], data->bands[col]);
    }

    strncpy(data->fileName, filename, sizeof(data->fileName) - 1);
    data->fileName[sizeof(data->fileName) - 1] = '\0';
    data->hasAudio = TRUE;
    data->positionMs = 0;
    FreeVec(wav);
    return TRUE;
}

static BOOL loadWaveformFromSamples(struct SpeechWaveformData *data,
                                    const UBYTE *pcm, ULONG dataLength,
                                    ULONG sampleRate, UWORD bits,
                                    UWORD channels, BOOL signedPcm,
                                    BOOL bigEndian) {
    ULONG frames;
    ULONG col;
    ULONG bytesPerFrame;

    if (pcm == NULL || dataLength == 0 || sampleRate == 0)
        return FALSE;
    bytesPerFrame = ((bits + 7) / 8) * channels;
    if (bytesPerFrame == 0)
        return FALSE;
    frames = dataLength / bytesPerFrame;
    if (frames == 0)
        return FALSE;

    memset(data->peaks, 0, sizeof(data->peaks));
    memset(data->bands, 0, sizeof(data->bands));
    data->cacheValid = FALSE;
    data->durationMs = (ULONG)(((unsigned long long)frames * 1000ULL) /
                               sampleRate);
    for (col = 0; col < WAVE_COLS; col++) {
        WORD window[FFT_N];
        ULONG start = (col * frames) / WAVE_COLS;
        ULONG end = ((col + 1) * frames) / WAVE_COLS;
        ULONG n;
        if (end <= start)
            end = start + 1;
        for (n = 0; n < FFT_N; n++) {
            ULONG idx = start + ((end - start) * n) / FFT_N;
            if (idx >= frames)
                idx = frames - 1;
            window[n] = sampleAt(pcm, idx, bits, channels, signedPcm,
                                 bigEndian);
        }
        analyzeColumn(window, &data->peaks[col], data->bands[col]);
    }
    data->hasAudio = TRUE;
    data->positionMs = 0;
    return TRUE;
}

static void redrawWaveform(Object *obj) {
    if (obj != NULL && _win(obj) != NULL)
        MUI_Redraw(obj, MADF_DRAWOBJECT);
}

void speechWaveformSetFile(Object *obj, CONST_STRPTR filename) {
    struct SpeechWaveformData *data;

    if (obj == NULL || speechWaveformClass == NULL)
        return;
    data = INST_DATA(speechWaveformClass->mcc_Class, obj);
    loadWaveformFile(data, filename);
    if (!data->hasAudio && filename != NULL && filename[0] != '\0')
        speechWaveformLoadPlayback(obj);
    else
        redrawWaveform(obj);
}

void speechWaveformLoadPlayback(Object *obj) {
    struct SpeechWaveformData *data;

    if (obj == NULL || speechWaveformClass == NULL)
        return;
    if (!speechPlaybackHasAudio())
        return;
    data = INST_DATA(speechWaveformClass->mcc_Class, obj);
    if (loadWaveformFromSamples(
            data, speechPlaybackSamples(), speechPlaybackSampleBytes(),
            speechPlaybackSampleRate(), speechPlaybackBitsPerSample(),
            speechPlaybackChannelCount(), TRUE, TRUE))
        redrawWaveform(obj);
}

static void addEventHandler(struct IClass *cl, Object *obj) {
    struct SpeechWaveformData *data = INST_DATA(cl, obj);
    Object *window = _win(obj);

    if (data->eventHandlerAdded || window == NULL)
        return;
    DoMethod(window, MUIM_Window_AddEventHandler, &data->eh);
    data->eventHandlerAdded = TRUE;
}

static void remEventHandler(struct IClass *cl, Object *obj) {
    struct SpeechWaveformData *data = INST_DATA(cl, obj);
    Object *window = _win(obj);

    if (!data->eventHandlerAdded)
        return;
    if (window != NULL)
        DoMethod(window, MUIM_Window_RemEventHandler, &data->eh);
    data->eventHandlerAdded = FALSE;
}

static ULONG seekFromX(struct SpeechWaveformData *data, Object *obj, LONG x) {
    LONG left = _mleft(obj);
    LONG width = _mwidth(obj);
    ULONG ms;

    if (width <= 0 || !data->hasAudio || data->durationMs == 0)
        return data->positionMs;
    if (x < left)
        x = left;
    if (x >= left + width)
        x = left + width - 1;
    ms = ((ULONG)(x - left) * data->durationMs) / (ULONG)width;
    if (ms > data->durationMs)
        ms = data->durationMs;
    return ms;
}

static void waveformPens(struct DrawInfo *dri, ULONG *shadow, ULONG *shine,
                         ULONG *wavePen, ULONG *headPen, ULONG *specPen);

static LONG waveformHeight(Object *obj) {
    LONG height = _mheight(obj);

    if (height > CTRL_BAR + 8)
        return height - CTRL_BAR;
    if (height > 8)
        return height - 8;
    return height;
}

static void buttonBox(Object *obj, UBYTE btn, LONG *x, LONG *y) {
    LONG index = (LONG)btn - 1;

    *x = _mleft(obj) + BTN_PAD + index * (BTN_W + BTN_GAP);
    *y = _mtop(obj) + waveformHeight(obj) + (CTRL_BAR - BTN_H) / 2;
}

static BOOL buttonEnabled(struct SpeechWaveformData *data, UBYTE btn) {
    if (!data->hasSpeech)
        return FALSE;
    if (btn == BTN_PLAYPAUSE)
        return TRUE;
    if (btn == BTN_STOP)
        return data->playing || data->paused || data->positionMs > 0;
    if (btn == BTN_REWIND)
        return data->playing || data->positionMs > 0;
    return FALSE;
}

static UBYTE buttonAt(Object *obj, LONG mx, LONG my) {
    UBYTE btn;
    LONG x;
    LONG y;

    for (btn = BTN_PLAYPAUSE; btn <= BTN_REWIND; btn++) {
        buttonBox(obj, btn, &x, &y);
        if (mx >= x && mx < x + BTN_W && my >= y && my < y + BTN_H)
            return btn;
    }
    return BTN_NONE;
}

static BOOL inWaveform(Object *obj, LONG mx, LONG my) {
    LONG left = _mleft(obj);
    LONG top = _mtop(obj);
    LONG width = _mwidth(obj);
    LONG height = waveformHeight(obj);

    return mx >= left && mx < left + width && my >= top && my < top + height;
}

static void formatPlayTime(UBYTE *buf, ULONG posMs, ULONG durMs) {
    ULONG pm = posMs / 60000UL;
    ULONG ps = (posMs / 1000UL) % 60UL;
    ULONG dm = durMs / 60000UL;
    ULONG ds = (durMs / 1000UL) % 60UL;
    UBYTE *p = buf;

    if (pm > 99)
        pm = 99;
    if (dm > 99)
        dm = 99;
    if (pm >= 10)
        *p++ = (UBYTE)('0' + (pm / 10));
    *p++ = (UBYTE)('0' + (pm % 10));
    *p++ = ':';
    *p++ = (UBYTE)('0' + (ps / 10));
    *p++ = (UBYTE)('0' + (ps % 10));
    *p++ = ' ';
    *p++ = '/';
    *p++ = ' ';
    if (dm >= 10)
        *p++ = (UBYTE)('0' + (dm / 10));
    *p++ = (UBYTE)('0' + (dm % 10));
    *p++ = ':';
    *p++ = (UBYTE)('0' + (ds / 10));
    *p++ = (UBYTE)('0' + (ds % 10));
    *p = '\0';
}

static void startIcon(struct RastPort *rp, ULONG pen) {
    SetDrMd(rp, JAM1);
    SetAPen(rp, pen);
}

static void drawIconPlay(struct RastPort *rp, LONG x, LONG y, ULONG pen) {
    static const UBYTE widths[10] = {2, 3, 5, 6, 8, 8, 6, 5, 3, 2};
    LONG row;
    LONG col;

    startIcon(rp, pen);
    for (row = 0; row < 10; row++) {
        for (col = 0; col < (LONG)widths[row]; col++)
            WritePixel(rp, x + 6 + col, y + 3 + row);
    }
}

static void drawIconPause(struct RastPort *rp, LONG x, LONG y, ULONG pen) {
    startIcon(rp, pen);
    RectFill(rp, x + 5, y + 3, x + 8, y + 12);
    RectFill(rp, x + 13, y + 3, x + 16, y + 12);
}

static void drawIconStop(struct RastPort *rp, LONG x, LONG y, ULONG pen) {
    startIcon(rp, pen);
    RectFill(rp, x + 6, y + 4, x + 15, y + 12);
}

static void drawIconRewind(struct RastPort *rp, LONG x, LONG y, ULONG pen) {
    static const UBYTE widths[10] = {2, 3, 5, 6, 8, 8, 6, 5, 3, 2};
    LONG row;
    LONG col;

    startIcon(rp, pen);
    RectFill(rp, x + 3, y + 3, x + 5, y + 12);
    for (row = 0; row < 10; row++) {
        LONG start = x + 7 + (8 - (LONG)widths[row]);
        for (col = 0; col < (LONG)widths[row]; col++)
            WritePixel(rp, start + col, y + 3 + row);
    }
}

static void drawControlButton(struct RastPort *rp, Object *obj,
                              struct SpeechWaveformData *data, UBYTE btn,
                              ULONG shine, ULONG shadow, ULONG fill) {
    LONG x;
    LONG y;
    BOOL enabled = buttonEnabled(data, btn);
    BOOL pressed = (data->pressedBtn == btn);
    ULONG iconPen;

    buttonBox(obj, btn, &x, &y);
    SetDrMd(rp, JAM1);
    SetAPen(rp, fill);
    RectFill(rp, x, y, x + BTN_W - 1, y + BTN_H - 1);
    SetAPen(rp, pressed ? shadow : shine);
    Move(rp, x, y + BTN_H - 1);
    Draw(rp, x, y);
    Draw(rp, x + BTN_W - 1, y);
    SetAPen(rp, pressed ? shine : shadow);
    Draw(rp, x + BTN_W - 1, y + BTN_H - 1);
    Draw(rp, x, y + BTN_H - 1);

    iconPen = enabled ? shine : shadow;
    if (btn == BTN_PLAYPAUSE) {
        if (data->playing)
            drawIconPause(rp, x, y, iconPen);
        else
            drawIconPlay(rp, x, y, iconPen);
    } else if (btn == BTN_STOP)
        drawIconStop(rp, x, y, iconPen);
    else
        drawIconRewind(rp, x, y, iconPen);
}

static void drawControlBar(struct RastPort *rp, Object *obj,
                           struct SpeechWaveformData *data,
                           struct DrawInfo *dri) {
    LONG left = _mleft(obj);
    LONG width = _mwidth(obj);
    LONG barTop = _mtop(obj) + waveformHeight(obj);
    LONG barBottom = _mtop(obj) + _mheight(obj) - 1;
    ULONG shadow;
    ULONG shine;
    ULONG wavePen;
    ULONG headPen;
    ULONG specPen;
    ULONG fill;
    ULONG face;
    UBYTE timeText[24];
    LONG textW;
    LONG textX;
    LONG textY;

    if (width <= 0 || barBottom < barTop)
        return;

    waveformPens(dri, &shadow, &shine, &wavePen, &headPen, &specPen);
    (void)wavePen;
    (void)headPen;
    (void)specPen;
    fill = dri != NULL ? dri->dri_Pens[FILLPEN] : 3;
    face = dri != NULL ? dri->dri_Pens[BACKGROUNDPEN] : 0;
    SetAPen(rp, fill);
    RectFill(rp, left, barTop, left + width - 1, barBottom);
    SetAPen(rp, shine);
    Move(rp, left, barTop);
    Draw(rp, left + width - 1, barTop);

    drawControlButton(rp, obj, data, BTN_PLAYPAUSE, shine, shadow, face);
    drawControlButton(rp, obj, data, BTN_STOP, shine, shadow, face);
    drawControlButton(rp, obj, data, BTN_REWIND, shine, shadow, face);

    formatPlayTime(timeText, data->positionMs, data->durationMs);
    SetAPen(rp, shine);
    SetDrMd(rp, JAM1);
    textW = TextLength(rp, (STRPTR)timeText, strlen((char *)timeText));
    textX = left + width - 4 - textW;
    if (rp->TxHeight > 0)
        textY = barTop + ((CTRL_BAR - rp->TxHeight) / 2) + rp->TxBaseline;
    else
        textY = barTop + 13;
    if (textX > left + BTN_PAD + 3 * (BTN_W + BTN_GAP)) {
        Move(rp, textX, textY);
        Text(rp, (STRPTR)timeText, strlen((char *)timeText));
    }
}

static ULONG commandForButton(struct SpeechWaveformData *data, UBYTE btn) {
    if (btn == BTN_PLAYPAUSE)
        return data->playing ? MUIV_SpeechWaveform_Command_Pause
                             : MUIV_SpeechWaveform_Command_Play;
    if (btn == BTN_STOP)
        return MUIV_SpeechWaveform_Command_Stop;
    if (btn == BTN_REWIND)
        return MUIV_SpeechWaveform_Command_Rewind;
    return 0;
}

static void waveformPens(struct DrawInfo *dri, ULONG *shadow, ULONG *shine,
                         ULONG *wavePen, ULONG *headPen, ULONG *specPen) {
    ULONG fill;

    *shadow = dri != NULL ? dri->dri_Pens[SHADOWPEN] : 1;
    *shine = dri != NULL ? dri->dri_Pens[SHINEPEN] : 2;
    fill = dri != NULL ? dri->dri_Pens[FILLPEN] : 3;
    *wavePen = greenPen ? greenPen : *shine;
    *headPen = yellowPen ? yellowPen : *shine;
    *specPen = bluePen ? bluePen : fill;
}

static void renderStaticWaveform(struct RastPort *rp, LONG left, LONG top,
                                 LONG width, LONG height,
                                 struct SpeechWaveformData *data,
                                 struct DrawInfo *dri) {
    LONG x;
    LONG mid;
    ULONG shadow;
    ULONG shine;
    ULONG wavePen;
    ULONG headPen;
    ULONG specPen;

    waveformPens(dri, &shadow, &shine, &wavePen, &headPen, &specPen);
    SetAPen(rp, shadow);
    RectFill(rp, left, top, left + width - 1, top + height - 1);

    if (!data->hasAudio) {
        SetAPen(rp, shine);
        Move(rp, left + 4, top + height / 2);
        Draw(rp, left + width - 5, top + height / 2);
        return;
    }

    mid = top + height / 2;
    for (x = 0; x < width; x++) {
        ULONG col = ((ULONG)x * WAVE_COLS) / (ULONG)width;
        LONG peak;
        LONG y0;
        LONG y1;
        LONG band;
        LONG bandH;

        if (col >= WAVE_COLS)
            col = WAVE_COLS - 1;
        peak = (LONG)data->peaks[col] * (height / 2 - 2) / 255;
        if (peak < 1)
            peak = 1;
        y0 = mid - peak;
        y1 = mid + peak;
        if (y0 < top + 1)
            y0 = top + 1;
        if (y1 > top + height - 2)
            y1 = top + height - 2;

        bandH = (y1 - y0 + 1) / WAVE_BANDS;
        if (bandH < 1)
            bandH = 1;
        for (band = 0; band < WAVE_BANDS; band++) {
            LONG by0 = y1 - (band + 1) * bandH;
            LONG by1 = y1 - band * bandH;
            UBYTE energy = data->bands[col][band];
            ULONG pen = shadow;
            if (energy > 160)
                pen = headPen;
            else if (energy > 80)
                pen = wavePen;
            else if (energy > 24)
                pen = specPen;
            if (by0 < y0)
                by0 = y0;
            if (by1 > y1)
                by1 = y1;
            if (by1 >= by0) {
                SetAPen(rp, pen);
                if (by1 > by0)
                    RectFill(rp, left + x, by0, left + x, by1);
                else
                    WritePixel(rp, left + x, by0);
            }
        }

        SetAPen(rp, wavePen);
        WritePixel(rp, left + x, y0);
        WritePixel(rp, left + x, y1);
    }
}

static void drawPlayhead(struct RastPort *rp, LONG left, LONG top, LONG width,
                         LONG height, struct SpeechWaveformData *data,
                         struct DrawInfo *dri) {
    LONG playX;
    ULONG shadow;
    ULONG shine;
    ULONG wavePen;
    ULONG headPen;
    ULONG specPen;

    if (!data->hasAudio || data->durationMs == 0 || width <= 0)
        return;
    waveformPens(dri, &shadow, &shine, &wavePen, &headPen, &specPen);
    playX = left + (LONG)((data->positionMs * (ULONG)width) / data->durationMs);
    if (playX < left)
        playX = left;
    if (playX > left + width - 1)
        playX = left + width - 1;
    SetAPen(rp, headPen);
    Move(rp, playX, top);
    Draw(rp, playX, top + height - 1);
}

static BOOL ensureWaveformCache(struct IClass *cl, Object *obj) {
    struct SpeechWaveformData *data = INST_DATA(cl, obj);
    struct RastPort *rp = _rp(obj);
    LONG width = _mwidth(obj);
    LONG height = waveformHeight(obj);
    struct BitMap *friendBm;
    ULONG depth;

    if (rp == NULL || rp->BitMap == NULL || width <= 0 || height <= 0)
        return FALSE;
    if (data->cacheValid && data->cacheBm != NULL &&
        data->cacheWidth == width && data->cacheHeight == height)
        return TRUE;

    freeWaveformCache(data);
    friendBm = rp->BitMap;
    depth = (ULONG)friendBm->Depth;
    if (depth < 1)
        depth = 1;
    data->cacheBm =
        AllocBitMap((ULONG)width, (ULONG)height, depth,
                    BMF_MINPLANES | BMF_CLEAR, friendBm);
    if (data->cacheBm == NULL)
        return FALSE;
    InitRastPort(&data->cacheRp);
    data->cacheRp.BitMap = data->cacheBm;
    data->cacheWidth = width;
    data->cacheHeight = height;
    renderStaticWaveform(&data->cacheRp, 0, 0, width, height, data, _dri(obj));
    WaitBlit();
    data->cacheValid = TRUE;
    return TRUE;
}

static void drawWaveform(struct IClass *cl, Object *obj) {
    struct SpeechWaveformData *data = INST_DATA(cl, obj);
    struct RastPort *rp = _rp(obj);
    struct DrawInfo *dri = _dri(obj);
    LONG left = _mleft(obj);
    LONG top = _mtop(obj);
    LONG width = _mwidth(obj);
    LONG height = waveformHeight(obj);

    if (rp == NULL || width <= 0 || _mheight(obj) <= 0)
        return;

    if (height > 0) {
        if (ensureWaveformCache(cl, obj)) {
            BltBitMapRastPort(data->cacheBm, 0, 0, rp, left, top, width,
                              height, 0xC0);
            WaitBlit();
        } else {
            renderStaticWaveform(rp, left, top, width, height, data, dri);
        }
        drawPlayhead(rp, left, top, width, height, data, dri);
    }
    drawControlBar(rp, obj, data, dri);
}

static SAVEDS ULONG mNew(struct IClass *cl, Object *obj, Msg msg) {
    if (!(obj = (Object *)DoSuperMethodA(cl, obj, msg)))
        return 0;
    {
        struct SpeechWaveformData *data = INST_DATA(cl, obj);
        memset(data, 0, sizeof(*data));
    }
    return (ULONG)obj;
}

static SAVEDS ULONG mGet(struct IClass *cl, Object *obj, struct opGet *msg) {
    struct SpeechWaveformData *data = INST_DATA(cl, obj);

    switch (msg->opg_AttrID) {
    case MUIA_SpeechWaveform_FileName:
        *msg->opg_Storage = (ULONG)data->fileName;
        return TRUE;
    case MUIA_SpeechWaveform_Position:
        *msg->opg_Storage = data->positionMs;
        return TRUE;
    case MUIA_SpeechWaveform_Duration:
        *msg->opg_Storage = data->durationMs;
        return TRUE;
    case MUIA_SpeechWaveform_Seek:
        *msg->opg_Storage = data->positionMs;
        return TRUE;
    case MUIA_SpeechWaveform_Playing:
        *msg->opg_Storage = (ULONG)data->playing;
        return TRUE;
    case MUIA_SpeechWaveform_Paused:
        *msg->opg_Storage = (ULONG)data->paused;
        return TRUE;
    case MUIA_SpeechWaveform_HasSpeech:
        *msg->opg_Storage = (ULONG)data->hasSpeech;
        return TRUE;
    case MUIA_SpeechWaveform_Command:
        *msg->opg_Storage = data->command;
        return TRUE;
    }
    return DoSuperMethodA(cl, obj, (Msg)msg);
}

static SAVEDS ULONG mSet(struct IClass *cl, Object *obj, struct opSet *msg) {
    struct SpeechWaveformData *data = INST_DATA(cl, obj);
    struct TagItem *tags;
    struct TagItem *tag;
    BOOL redraw = FALSE;
    BOOL update = FALSE;

    for (tags = msg->ops_AttrList; (tag = NextTagItem(&tags));) {
        switch (tag->ti_Tag) {
        case MUIA_SpeechWaveform_FileName:
            loadWaveformFile(data, (CONST_STRPTR)tag->ti_Data);
            data->cacheValid = FALSE;
            redraw = TRUE;
            break;
        case MUIA_SpeechWaveform_Position:
            if (data->positionMs != tag->ti_Data) {
                data->positionMs = tag->ti_Data;
                if (data->positionMs > data->durationMs)
                    data->positionMs = data->durationMs;
                update = TRUE;
            }
            break;
        case MUIA_SpeechWaveform_Playing:
            if (data->playing != (BOOL)tag->ti_Data) {
                data->playing = (BOOL)tag->ti_Data;
                update = TRUE;
            }
            break;
        case MUIA_SpeechWaveform_Paused:
            if (data->paused != (BOOL)tag->ti_Data) {
                data->paused = (BOOL)tag->ti_Data;
                update = TRUE;
            }
            break;
        case MUIA_SpeechWaveform_HasSpeech:
            if (data->hasSpeech != (BOOL)tag->ti_Data) {
                data->hasSpeech = (BOOL)tag->ti_Data;
                update = TRUE;
            }
            break;
        case MUIA_SpeechWaveform_Command:
            data->command = tag->ti_Data;
            break;
        }
    }

    if (_win(obj) != NULL) {
        if (redraw)
            MUI_Redraw(obj, MADF_DRAWOBJECT);
        else if (update)
            MUI_Redraw(obj, MADF_DRAWUPDATE);
    }

    return DoSuperMethodA(cl, obj, (Msg)msg);
}

static SAVEDS ULONG mSetup(struct IClass *cl, Object *obj, Msg msg) {
    struct SpeechWaveformData *data = INST_DATA(cl, obj);

    if (!(DoSuperMethodA(cl, obj, msg)))
        return FALSE;

    data->eh.ehn_Class = cl;
    data->eh.ehn_Object = obj;
    data->eh.ehn_Events = IDCMP_MOUSEBUTTONS | IDCMP_MOUSEMOVE;
    data->eh.ehn_Flags = MUI_EHF_GUIMODE;
    data->eh.ehn_Priority = 0;
    addEventHandler(cl, obj);
    return TRUE;
}

static SAVEDS ULONG mCleanup(struct IClass *cl, Object *obj, Msg msg) {
    struct SpeechWaveformData *data = INST_DATA(cl, obj);
    remEventHandler(cl, obj);
    freeWaveformCache(data);
    return DoSuperMethodA(cl, obj, msg);
}

static SAVEDS ULONG mAskMinMax(struct IClass *cl, Object *obj,
                               struct MUIP_AskMinMax *msg) {
    DoSuperMethodA(cl, obj, (Msg)msg);
    msg->MinMaxInfo->MinWidth += 160;
    msg->MinMaxInfo->DefWidth += 240;
    msg->MinMaxInfo->MaxWidth = MUI_MAXMAX;
    msg->MinMaxInfo->MinHeight += 68;
    msg->MinMaxInfo->DefHeight += 116;
    msg->MinMaxInfo->MaxHeight = MUI_MAXMAX;
    return 0;
}

static SAVEDS ULONG mDraw(struct IClass *cl, Object *obj,
                          struct MUIP_Draw *msg) {
    DoSuperMethodA(cl, obj, (Msg)msg);
    drawWaveform(cl, obj);
    return 0;
}

static SAVEDS ULONG mHandleEvent(struct IClass *cl, Object *obj,
                                 struct MUIP_HandleEvent *msg) {
    struct SpeechWaveformData *data = INST_DATA(cl, obj);
    struct IntuiMessage *imsg = msg->imsg;
    LONG mx;
    LONG my;

    if (imsg == NULL)
        return DoSuperMethodA(cl, obj, (Msg)msg);

    mx = imsg->MouseX;
    my = imsg->MouseY;

    if (imsg->Class == IDCMP_MOUSEBUTTONS) {
        if (imsg->Code == SELECTDOWN) {
            UBYTE btn = buttonAt(obj, mx, my);
            if (btn != BTN_NONE) {
                if (!buttonEnabled(data, btn))
                    return MUI_EventHandlerRC_Eat;
                data->pressedBtn = btn;
                data->dragging = FALSE;
                MUI_Redraw(obj, MADF_DRAWUPDATE);
                return MUI_EventHandlerRC_Eat;
            }
            if (data->hasAudio && inWaveform(obj, mx, my)) {
                ULONG ms = seekFromX(data, obj, mx);
                data->dragging = TRUE;
                data->pressedBtn = BTN_NONE;
                data->positionMs = ms;
                MUI_Redraw(obj, MADF_DRAWUPDATE);
                set(obj, MUIA_SpeechWaveform_Seek, ms);
                return MUI_EventHandlerRC_Eat;
            }
        } else if (imsg->Code == SELECTUP) {
            UBYTE pressed = data->pressedBtn;
            data->dragging = FALSE;
            data->pressedBtn = BTN_NONE;
            if (pressed != BTN_NONE) {
                MUI_Redraw(obj, MADF_DRAWUPDATE);
                if (buttonAt(obj, mx, my) == pressed &&
                    buttonEnabled(data, pressed)) {
                    ULONG command = commandForButton(data, pressed);
                    if (command != 0)
                        set(obj, MUIA_SpeechWaveform_Command, command);
                }
                return MUI_EventHandlerRC_Eat;
            }
        }
    } else if (imsg->Class == IDCMP_MOUSEMOVE && data->dragging &&
               data->hasAudio) {
        ULONG ms = seekFromX(data, obj, mx);
        if (ms != data->positionMs) {
            data->positionMs = ms;
            MUI_Redraw(obj, MADF_DRAWUPDATE);
            set(obj, MUIA_SpeechWaveform_Seek, ms);
        }
        return MUI_EventHandlerRC_Eat;
    }

    return DoSuperMethodA(cl, obj, (Msg)msg);
}

DISPATCHER(SpeechWaveformDispatcher) {
    switch (msg->MethodID) {
    case OM_NEW:
        return (mNew(cl, obj, (APTR)msg));
    case OM_GET:
        return (mGet(cl, obj, (APTR)msg));
    case OM_SET:
        return (mSet(cl, obj, (APTR)msg));
    case MUIM_Setup:
        return (mSetup(cl, obj, (APTR)msg));
    case MUIM_Cleanup:
        return (mCleanup(cl, obj, (APTR)msg));
    case MUIM_AskMinMax:
        return (mAskMinMax(cl, obj, (APTR)msg));
    case MUIM_Draw:
        return (mDraw(cl, obj, (APTR)msg));
    case MUIM_HandleEvent:
        return (mHandleEvent(cl, obj, (APTR)msg));
    }
    return DoSuperMethodA(cl, obj, msg);
}

LONG createSpeechWaveformClass(void) {
    if (speechWaveformClass != NULL)
        return RETURN_OK;

    if (!(speechWaveformClass = MUI_CreateCustomClass(
              NULL, MUIC_Area, NULL, sizeof(struct SpeechWaveformData),
              ENTRY(SpeechWaveformDispatcher))))
        return RETURN_ERROR;
    return RETURN_OK;
}

void deleteSpeechWaveformClass(void) {
    if (speechWaveformClass == NULL)
        return;
    MUI_DeleteCustomClass(speechWaveformClass);
    speechWaveformClass = NULL;
}
