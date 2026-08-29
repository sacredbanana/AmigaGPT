#include <clib/alib_protos.h>
#include <dos/dos.h>
#include <intuition/intuition.h>
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

static void clearWaveform(struct SpeechWaveformData *data) {
    memset(data->peaks, 0, sizeof(data->peaks));
    memset(data->bands, 0, sizeof(data->bands));
    data->hasAudio = FALSE;
    data->durationMs = 0;
    data->positionMs = 0;
    data->fileName[0] = '\0';
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

static void drawWaveform(struct IClass *cl, Object *obj) {
    struct SpeechWaveformData *data = INST_DATA(cl, obj);
    struct RastPort *rp = _rp(obj);
    struct DrawInfo *dri = _dri(obj);
    LONG left = _mleft(obj);
    LONG top = _mtop(obj);
    LONG width = _mwidth(obj);
    LONG height = _mheight(obj);
    LONG x;
    LONG mid;
    LONG playX;
    ULONG shadow;
    ULONG shine;
    ULONG fill;
    ULONG wavePen;
    ULONG headPen;
    ULONG specPen;

    if (rp == NULL || width <= 0 || height <= 0)
        return;

    shadow = dri != NULL ? dri->dri_Pens[SHADOWPEN] : 1;
    shine = dri != NULL ? dri->dri_Pens[SHINEPEN] : 2;
    fill = dri != NULL ? dri->dri_Pens[FILLPEN] : 3;
    wavePen = greenPen ? greenPen : shine;
    headPen = yellowPen ? yellowPen : shine;
    specPen = bluePen ? bluePen : fill;

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
        ULONG played;
        LONG peak;
        if (col >= WAVE_COLS)
            col = WAVE_COLS - 1;
        played = data->durationMs > 0 &&
                 ((ULONG)x * data->durationMs) / (ULONG)width <
                     data->positionMs;
        peak = (LONG)data->peaks[col] * (height / 2 - 2) / 255;
        LONG y0;
        LONG y1;
        LONG band;
        LONG bandH;

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
            if (played && pen == shadow)
                pen = fill;
            if (by0 < y0)
                by0 = y0;
            if (by1 > y1)
                by1 = y1;
            if (by1 >= by0) {
                SetAPen(rp, pen);
                WritePixel(rp, left + x, (by0 + by1) / 2);
                if (by1 > by0)
                    RectFill(rp, left + x, by0, left + x, by1);
            }
        }

        SetAPen(rp, played ? headPen : wavePen);
        WritePixel(rp, left + x, y0);
        WritePixel(rp, left + x, y1);
    }

    if (data->durationMs > 0) {
        playX = left + (LONG)((data->positionMs * (ULONG)width) /
                              data->durationMs);
        if (playX < left)
            playX = left;
        if (playX > left + width - 1)
            playX = left + width - 1;
        SetAPen(rp, headPen);
        Move(rp, playX, top);
        Draw(rp, playX, top + height - 1);
    }
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
    }
    return DoSuperMethodA(cl, obj, (Msg)msg);
}

static SAVEDS ULONG mSet(struct IClass *cl, Object *obj, struct opSet *msg) {
    struct SpeechWaveformData *data = INST_DATA(cl, obj);
    struct TagItem *tags;
    struct TagItem *tag;
    BOOL redraw = FALSE;

    for (tags = msg->ops_AttrList; (tag = NextTagItem(&tags));) {
        switch (tag->ti_Tag) {
        case MUIA_SpeechWaveform_FileName:
            loadWaveformFile(data, (CONST_STRPTR)tag->ti_Data);
            redraw = TRUE;
            break;
        case MUIA_SpeechWaveform_Position:
            if (data->positionMs != tag->ti_Data) {
                data->positionMs = tag->ti_Data;
                if (data->positionMs > data->durationMs)
                    data->positionMs = data->durationMs;
                redraw = TRUE;
            }
            break;
        }
    }

    if (redraw && _win(obj) != NULL)
        MUI_Redraw(obj, MADF_DRAWOBJECT);

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
    remEventHandler(cl, obj);
    return DoSuperMethodA(cl, obj, msg);
}

static SAVEDS ULONG mAskMinMax(struct IClass *cl, Object *obj,
                               struct MUIP_AskMinMax *msg) {
    DoSuperMethodA(cl, obj, (Msg)msg);
    msg->MinMaxInfo->MinWidth += 80;
    msg->MinMaxInfo->DefWidth += 200;
    msg->MinMaxInfo->MaxWidth = MUI_MAXMAX;
    msg->MinMaxInfo->MinHeight += 48;
    msg->MinMaxInfo->DefHeight += 96;
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
        if (imsg->Code == SELECTDOWN && data->hasAudio &&
            mx >= _mleft(obj) && mx < _mleft(obj) + _mwidth(obj) &&
            my >= _mtop(obj) && my < _mtop(obj) + _mheight(obj)) {
            ULONG ms = seekFromX(data, obj, mx);
            data->dragging = TRUE;
            data->positionMs = ms;
            MUI_Redraw(obj, MADF_DRAWOBJECT);
            set(obj, MUIA_SpeechWaveform_Seek, ms);
            return MUI_EventHandlerRC_Eat;
        }
        if (imsg->Code == SELECTUP)
            data->dragging = FALSE;
    } else if (imsg->Class == IDCMP_MOUSEMOVE && data->dragging &&
               data->hasAudio) {
        ULONG ms = seekFromX(data, obj, mx);
        if (ms != data->positionMs) {
            data->positionMs = ms;
            MUI_Redraw(obj, MADF_DRAWOBJECT);
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
