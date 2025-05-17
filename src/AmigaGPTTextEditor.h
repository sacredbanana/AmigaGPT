#ifndef AMIGAGPTTEXTEDITOR_H
#define AMIGAGPTTEXTEDITOR_H

#define AmigaGPTTextEditor_Dummy (0xbd000000UL)
#define MUIA_AmigaGPTTextEditor_SubmitHook (AmigaGPTTextEditor_Dummy + 0x02)

#define MUIC_AmigaGPTTextEditor amigaGPTTextEditorClass->mcc_Class

#if defined(__AROS__) && !defined(NO_INLINE_STDARG)
#define AmigaGPTTextEditorObject MUIOBJMACRO_START(MUIC_AmigaGPTTextEditor)
#else
#define AmigaGPTTextEditorObject MUI_NewObject(MUIC_AmigaGPTTextEditor
#endif

/**
 * @brief The AmigaGPTTextEditor class
 * @note This custom class is used to create a text editor that can run a hook
 * when the user hits return. The base class's normal return handler is now
 * called when Right Shift + Return is pressed
 */
extern struct MUI_CustomClass *amigaGPTTextEditorClass;

/**
 * @brief Create the AmigaGPTTextEditor class
 * @return RETURN_OK if successful, RETURN_ERROR otherwise
 */
LONG createAmigaGPTTextEditor();

/**
 * @brief Delete the AmigaGPTTextEditor class
 */
void deleteAmigaGPTTextEditor();
#endif