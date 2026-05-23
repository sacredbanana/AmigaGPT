#ifdef __MORPHOS__

#include <libraries/mui.h>
#include <proto/exec.h>
#include <proto/muimaster.h>
#include <Scintilla/Scintilla.h>
#include <mui/Scintilla_mcc.h>
#include "CodeBlocksScintilla.h"

/*
 * MUIP_Scintilla_Command is in the public header but without a #define.
 * Slot +2 is between MUIA_Scintilla_ActiveEditor (+1) and MUIA_Scintilla_Notify (+3).
 * (Runtime probe with "ok" confirmed Command works; probe removed — it froze the app.)
 */
#ifndef MUIM_Scintilla_Command
#define MUIM_Scintilla_Command (MUIA_Scintilla_dummy + 2)
#endif

/* MorphOS Scintilla.guide: 0 = graphics.library, 1 = TTEngine (Unicode). */
#define CODEBLOCKS_SCINTILLA_FONTQUALITY_TTENGINE 1

/*
 * TTEngine family name (exact string from Flow Studio / TT_ObtainFamilyList).
 * SCI_SETFONTQUALITY(1) required. Monospace candidates on MorphOS (examples):
 *   DejaVu Sans Mono, Bitstream Vera Sans Mono, Liberation Mono,
 *   Noto Sans Mono, Roboto Mono, Luxi Mono (often shown as "Lux…"), Arimo.
 * Proportional TTEngine fonts (Arial, DejaVu Sans, …) are wrong for code columns.
 * Default: DejaVu Sans Mono (validated in Flow Studio with UTF test file).
 */
#define CODEBLOCKS_SCINTILLA_FONT_FACE "DejaVu Sans Mono"
#define CODEBLOCKS_SCINTILLA_FONT_SIZE_POINTS 12

static sptr_t scintillaCommand(Object *sci, unsigned int iMessage, uptr_t wParam,
                               sptr_t lParam) {
    struct MUIP_Scintilla_Command cmd;

    if (sci == NULL) {
        return 0;
    }
    cmd.MethodID = MUIM_Scintilla_Command;
    cmd.iMessage = (LONG)iMessage;
    cmd.wParam = (LONG)wParam;
    cmd.lParam = (LONG)lParam;
    return (sptr_t)DoMethodA(sci, (Msg)&cmd);
}

sptr_t codeBlocksScintillaCommand(Object *sci, unsigned int iMessage,
                                  uptr_t wParam, sptr_t lParam) {
    return scintillaCommand(sci, iMessage, wParam, lParam);
}

static void codeBlocksScintillaApplyViewerFont(Object *sci, BOOL clearAll) {
    static const char fontFace[] = CODEBLOCKS_SCINTILLA_FONT_FACE;

    if (sci == NULL) {
        return;
    }

    scintillaCommand(sci, SCI_STYLESETFONT, STYLE_DEFAULT, (sptr_t)fontFace);
    scintillaCommand(sci, SCI_STYLESETSIZE, STYLE_DEFAULT,
                     CODEBLOCKS_SCINTILLA_FONT_SIZE_POINTS);
    scintillaCommand(sci, SCI_STYLESETFONT, 0, (sptr_t)fontFace);
    scintillaCommand(sci, SCI_STYLESETSIZE, 0, CODEBLOCKS_SCINTILLA_FONT_SIZE_POINTS);
    if (clearAll) {
        scintillaCommand(sci, SCI_STYLECLEARALL, 0, 0);
    }
}

ULONG codeBlocksScintillaDocumentLength(Object *sci) {
    return (ULONG)scintillaCommand(sci, SCI_GETLENGTH, 0, 0);
}

void codeBlocksScintillaSetUtf8Text(Object *sci, const char *utf8) {
    if (sci == NULL) {
        return;
    }
    if (utf8 == NULL) {
        utf8 = "";
    }

    scintillaCommand(sci, SCI_SETFONTQUALITY, CODEBLOCKS_SCINTILLA_FONTQUALITY_TTENGINE, 0);
    scintillaCommand(sci, SCI_SETCODEPAGE, SC_CP_UTF8, 0);
    codeBlocksScintillaApplyViewerFont(sci, FALSE);
    scintillaCommand(sci, SCI_SETREADONLY, 0, 0);
    scintillaCommand(sci, SCI_SETTEXT, 0, (sptr_t)utf8);
    scintillaCommand(sci, SCI_SETREADONLY, 1, 0);
}

void codeBlocksScintillaInitViewer(Object *sci) {
    if (sci == NULL) {
        return;
    }
    scintillaCommand(sci, SCI_SETFONTQUALITY, CODEBLOCKS_SCINTILLA_FONTQUALITY_TTENGINE, 0);
    scintillaCommand(sci, SCI_SETCODEPAGE, SC_CP_UTF8, 0);
    codeBlocksScintillaApplyViewerFont(sci, TRUE);
    scintillaCommand(sci, SCI_SETUNDOCOLLECTION, 0, 0);
}

#endif /* __MORPHOS__ */
