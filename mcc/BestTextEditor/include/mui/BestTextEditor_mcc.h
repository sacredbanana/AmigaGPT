#ifndef BESTTEXTEDITOR_MCC_H
#define BESTTEXTEDITOR_MCC_H

/*
 * BestTextEditor.mcc -- a multi-line text editing and display MUI class
 *
 * A from-scratch replacement for TextEditor.mcc and ActionTextEditor.mcc. It
 * does not subclass either of them: it is a direct subclass of Area.
 *
 * Compatibility
 *   Every MUIA_TextEditor_* attribute and MUIM_TextEditor_* method from
 *   <mui/TextEditor_mcc.h> is answered by this class with the same ID, so an
 *   object of this class can be used anywhere a TextEditor.mcc object was.
 *   MUIA_ActionTextEditor_SubmitHook is accepted as a synonym of
 *   MUIA_BestTextEditor_SubmitHook.
 *
 * Extensions
 *   - Native Markdown rendering (headings, emphasis, strike-through, inline
 *     code, fenced code blocks, block quotes, bullet/numbered/task lists,
 *     rules, tables, links and images). The Markdown stays in the text; read
 *     only objects hide the markup characters, editable ones dim them.
 *   - Chat bubbles: paragraphs can be grouped into rounded "bubbles" aligned
 *     left, right or centred, in the style of a messaging app.
 *   - Inline images loaded through datatypes, and clickable links/images.
 *   - Pixel smooth scrolling, double buffered line rendering, incremental
 *     re-layout of only the paragraphs that changed, and a fast append path
 *     for streaming text.
 *   - Justified text, overwrite mode and keyboard macros (the parts of the
 *     TextEditor.mcc API it documents as "not yet implemented").
 *   - Unicode: the text is kept as UTF-8. Characters of the system codeset
 *     are drawn with the Amiga font, common characters it lacks (curly
 *     quotes, dashes, ellipsis, arrows, box drawing...) with look-alikes,
 *     and emoji in colour, including skin tones, flags and ZWJ sequences,
 *     from BestTextEditor.emoji (Twemoji graphics, CC-BY 4.0), which sits
 *     next to the class. Anything else shows as a box.
 *
 * Text encoding
 *   By default every string given to or returned by the class (contents,
 *   inserted and exported text, ARexx commands, search strings, keywords,
 *   links) is in the system codeset, exactly as with TextEditor.mcc, and
 *   cursor columns count the characters of that text. Characters the
 *   system codeset cannot hold come out as '?' (or their look-alike).
 *   With MUIA_BestTextEditor_UTF8 set, those strings are UTF-8 and cursor
 *   columns and indexes are byte offsets into the UTF-8 text. Typed text
 *   and the clipboard are converted either way: copying writes an FTXT
 *   CHRS chunk in the system codeset and a UTF8 chunk, and pasting prefers
 *   the UTF8 chunk.
 *
 * Extra escape sequences understood by the import (at the start of a line):
 *   ESC [B:n]    start a new bubble using bubble style n (1..8); the bubble
 *                continues over the following lines
 *   ESC [B:nl]   as above, forcing the bubble to the left (r = right,
 *                c = centre, f = full width)
 *   ESC [B:0]    end the current bubble
 *   ESC [M:1]    render the following lines as Markdown
 *   ESC [M:0]    stop rendering the following lines as Markdown
 *
 * Appearance
 *   Fonts, the background and text colour of editable and of read only
 *   editors, accent colours and the eight bubble styles can be set by the
 *   user on the BestTextEditor page of MUI preferences (BestTextEditor.mcp).
 *   Whether an object counts as read only is decided by
 *   MUIA_TextEditor_ReadOnly at creation, like its frame. Attributes given by
 *   the application (MUIA_Font, MUIA_Background,
 *   MUIA_BestTextEditor_CodeFont, MUIA_BestTextEditor_BubbleStyles) take
 *   precedence over the preferences.
 *
 * The tag base 0xbd002000 belongs to this class.
 */

#include <exec/types.h>
#include <libraries/mui.h>
#include <mui/TextEditor_mcc.h>

#define BestTextEditor_Dummy (0xbd002000UL)

/* Attributes */
#define MUIA_BestTextEditor_SubmitHook      (BestTextEditor_Dummy + 0x01) /* ISG struct Hook *  */
#define MUIA_BestTextEditor_Markdown        (BestTextEditor_Dummy + 0x02) /* ISG BOOL           */
#define MUIA_BestTextEditor_HideMarkup      (BestTextEditor_Dummy + 0x03) /* ISG BOOL           */
#define MUIA_BestTextEditor_LinkHook        (BestTextEditor_Dummy + 0x04) /* IS. struct Hook *  */
#define MUIA_BestTextEditor_ClickedLink     (BestTextEditor_Dummy + 0x05) /* ..G STRPTR         */
#define MUIA_BestTextEditor_StickToBottom   (BestTextEditor_Dummy + 0x06) /* ISG BOOL           */
#define MUIA_BestTextEditor_MaxLength       (BestTextEditor_Dummy + 0x07) /* ISG ULONG          */
#define MUIA_BestTextEditor_BubbleStyles    (BestTextEditor_Dummy + 0x08) /* IS. struct BestTextBubbleStyle * */
#define MUIA_BestTextEditor_ImageHook       (BestTextEditor_Dummy + 0x09) /* IS. struct Hook *  */
#define MUIA_BestTextEditor_MaxImageHeight  (BestTextEditor_Dummy + 0x0a) /* ISG ULONG          */
#define MUIA_BestTextEditor_Length          (BestTextEditor_Dummy + 0x0b) /* ..G ULONG          */
#define MUIA_BestTextEditor_Paragraphs      (BestTextEditor_Dummy + 0x0c) /* ..G ULONG          */
#define MUIA_BestTextEditor_AtBottom        (BestTextEditor_Dummy + 0x0d) /* ..G BOOL           */
#define MUIA_BestTextEditor_SubmitEmpty     (BestTextEditor_Dummy + 0x0e) /* ISG BOOL           */
#define MUIA_BestTextEditor_CodeFont        (BestTextEditor_Dummy + 0x0f) /* I.. struct TextFont * */
#define MUIA_BestTextEditor_UTF8            (BestTextEditor_Dummy + 0x10) /* ISG BOOL           */

/* Methods */
#define MUIM_BestTextEditor_Append          (BestTextEditor_Dummy + 0x40)
#define MUIM_BestTextEditor_ScrollTo        (BestTextEditor_Dummy + 0x41)
#define MUIM_BestTextEditor_MacroRecord     (BestTextEditor_Dummy + 0x42)

struct MUIP_BestTextEditor_Append   { ULONG MethodID; CONST_STRPTR text; LONG length; ULONG flags; };
struct MUIP_BestTextEditor_ScrollTo { ULONG MethodID; LONG where; };

/* Flags for MUIM_BestTextEditor_Append */
#define MUIF_BestTextEditor_Append_Raw      (1 << 0) /* no escape sequences */
/* The text of MUIM_BestTextEditor_Append is in the same encoding as all
 * other text (MUIA_BestTextEditor_UTF8). In UTF-8 a character may be split
 * between two calls: it shows correctly once its last byte arrives. */
#define MUIF_BestTextEditor_Append_NoScroll (1 << 1) /* never follow the end */

/* Values for MUIM_BestTextEditor_ScrollTo */
#define MUIV_BestTextEditor_ScrollTo_Top    (-1)
#define MUIV_BestTextEditor_ScrollTo_Bottom (-2)
#define MUIV_BestTextEditor_ScrollTo_Cursor (-3)

/* Bubble styles (MUIA_BestTextEditor_BubbleStyles, ESC [B:n]) */
#define MUIV_BestTextEditor_Bubble_Left     1
#define MUIV_BestTextEditor_Bubble_Right    2
#define MUIV_BestTextEditor_Bubble_Center   3
#define MUIV_BestTextEditor_Bubble_Full     4
#define MUIV_BestTextEditor_Bubble_End      0   /* terminates the array */

#define MUIV_BestTextEditor_Color_Auto      0xffffffffUL

#define BESTTEXTEDITOR_MAX_BUBBLE_STYLES    8

struct BestTextBubbleStyle {
    ULONG side;       /* MUIV_BestTextEditor_Bubble_* */
    ULONG background; /* 0x00RRGGBB */
    ULONG text;       /* 0x00RRGGBB or MUIV_BestTextEditor_Color_Auto */
    ULONG maxWidth;   /* percent of the view width, 0 = default */
};

/* Message passed in A1 to MUIA_BestTextEditor_LinkHook */
struct BestTextLinkMessage {
    CONST_STRPTR url;     /* the link target or image source */
    CONST_STRPTR text;    /* the visible link text (may be NULL) */
    ULONG qualifier;      /* input qualifiers of the click */
    BOOL image;           /* TRUE when an image was clicked */
};

/* Message passed in A1 to MUIA_BestTextEditor_ImageHook. Return a file name
 * the datatypes system can load, or NULL to show the alternative text. */
struct BestTextImageMessage {
    CONST_STRPTR source;  /* the image source as written in the text */
};

#define MUIC_BestTextEditor "BestTextEditor.mcc"
#define MUIC_BestTextEditorP "BestTextEditor.mcp"

#define BestTextEditorObject MUI_NewObject(MUIC_BestTextEditor

#endif /* BESTTEXTEDITOR_MCC_H */
