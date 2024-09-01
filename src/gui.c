#ifdef __AMIGAOS3__ || __AMIGAOS4__
#include "amiga_compiler.h"
#endif
#include <datatypes/datatypes.h>
#include <datatypes/datatypesclass.h>
#include <datatypes/pictureclass.h>
#include <dos/dos.h>
#include <exec/exec.h>
#include <exec/execbase.h>
#include <exec/lists.h>
#include <graphics/modeid.h>
#include <graphics/text.h>
#include <intuition/intuition.h>
#include <json-c/json.h>
#include <libraries/amigaguide.h>
#include <libraries/asl.h>
#include <libraries/gadtools.h>
#include <libraries/mui.h>
#include <mui/Aboutbox_mcc.h>
#include <mui/NList_mcc.h>
#include <mui/NListview_mcc.h>
#include <mui/TextEditor_mcc.h>
#include <proto/amigaguide.h>
#include <proto/asl.h>
#include <proto/datatypes.h>
#include <proto/dos.h>
#include <proto/exec.h>
#include <proto/gadtools.h>
#include <proto/graphics.h>
#include <proto/intuition.h>
#include <proto/muimaster.h>
#include <proto/utility.h>
#include <proto/window.h>
#include <stdio.h>
#include <SDI_hook.h>
#include <utility/utility.h>
#include "config.h"
#include "gui.h"
#include "version.h"

#define HELP_KEY 0x5F
#define SCREEN_SELECT_WINDOW_ID 0
#define MAIN_WINDOW_ID 1
#define SEND_MESSAGE_BUTTON_ID 3
#define TEXT_INPUT_TEXT_EDITOR_ID 4
#define CHAT_OUTPUT_TEXT_EDITOR_ID 5
#define CHAT_OUTPUT_SCROLLER_ID 6
#define STATUS_BAR_ID 7
#define CONVERSATION_LIST_BROWSER_ID 8
#define NEW_CHAT_BUTTON_ID 9
#define DELETE_CHAT_BUTTON_ID 10
#define CREATE_IMAGE_BUTTON_ID 11
#define CLICKTAB_MODE_SELECTION_ID 12
#define IMAGE_LIST_BROWSER_ID 13
#define NEW_IMAGE_BUTTON_ID 14
#define DELETE_IMAGE_BUTTON_ID 15
#define OPEN_SMALL_IMAGE_BUTTON_ID 16
#define OPEN_MEDIUM_IMAGE_BUTTON_ID 17
#define OPEN_LARGE_IMAGE_BUTTON_ID 18
#define OPEN_ORIGINAL_IMAGE_BUTTON_ID 19
#define SAVE_COPY_BUTTON_ID 20
#define MODE_SELECT_RADIO_BUTTON_ID 21

// Actions
enum {
	STARTUP_OPTIONS_OK_BUTTON_PRESS = 1
};

#define NULL_ID 0

#define MODE_SELECTION_TAB_CHAT_ID 0
#define MODE_SELECTION_TAB_IMAGE_GENERATION_ID 1

// Menu item IDs
enum {
	MENU_ITEM_ABOUT_AMIGAGPT = 1,
	MENU_ITEM_ABOUT_MUI,
	MENU_ITEM_QUIT,
	MENU_ITEM_SPEECH_ACCENT,
	MENU_ITEM_SPEECH_ENABLED,
	MENU_ITEM_CHAT_FONT,
	MENU_ITEM_UI_FONT,
	MENU_ITEM_SPEECH_SYSTEM_34,
	MENU_ITEM_SPEECH_SYSTEM_37,
	MENU_ITEM_SPEECH_SYSTEM_FLITE,
	MENU_ITEM_SPEECH_SYSTEM_OPENAI,
	MENU_ITEM_CUT,
	MENU_ITEM_COPY,
	MENU_ITEM_PASTE,
	MENU_ITEM_CLEAR,
	MENU_ITEM_SELECT_ALL,
	MENU_ITEM_CHAT_MODEL_GPT_4o,
	MENU_ITEM_CHAT_MODEL_GPT_4o_2024_05_13,
	MENU_ITEM_CHAT_MODEL_GPT_4o_MINI,
	MENU_ITEM_CHAT_MODEL_GPT_4o_MINI_2024_07_18,
	MENU_ITEM_CHAT_MODEL_GPT_4_TURBO,
	MENU_ITEM_CHAT_MODEL_GPT_4_TURBO_2024_04_09,
	MENU_ITEM_CHAT_MODEL_GPT_4_TURBO_PREVIEW,
	MENU_ITEM_CHAT_MODEL_GPT_4_0125_PREVIEW,
	MENU_ITEM_CHAT_MODEL_GPT_4_1106_PREVIEW,
	MENU_ITEM_CHAT_MODEL_GPT_4,
	MENU_ITEM_CHAT_MODEL_GPT_4_0613,
	MENU_ITEM_CHAT_MODEL_GPT_3_5_TURBO,
	MENU_ITEM_CHAT_MODEL_GPT_3_5_TURBO_0125,
	MENU_ITEM_CHAT_MODEL_GPT_3_5_TURBO_1106,
	MENU_ITEM_CHAT_MODEL,
	MENU_ITEM_SPEECH_SYSTEM,
	MENU_ITEM_OPENAI_API_KEY,
	MENU_ITEM_VIEW_DOCUMENTATION,
	MENU_ITEM_SPEECH_FLITE_VOICE,
	MENU_ITEM_SPEECH_FLITE_VOICE_AWB,
	MENU_ITEM_SPEECH_FLITE_VOICE_KAL,
	MENU_ITEM_SPEECH_FLITE_VOICE_KAL16,
	MENU_ITEM_SPEECH_FLITE_VOICE_RMS,
	MENU_ITEM_SPEECH_FLITE_VOICE_SLT,
	MENU_ITEM_SPEECH_OPENAI_VOICE,
	MENU_ITEM_SPEECH_OPENAI_VOICE_ALLOY,
	MENU_ITEM_SPEECH_OPENAI_VOICE_ECHO,
	MENU_ITEM_SPEECH_OPENAI_VOICE_FABLE,
	MENU_ITEM_SPEECH_OPENAI_VOICE_ONYX,
	MENU_ITEM_SPEECH_OPENAI_VOICE_NOVA,
	MENU_ITEM_SPEECH_OPENAI_VOICE_SHIMMER,
	MENU_ITEM_SPEECH_OPENAI_MODEL,
	MENU_ITEM_SPEECH_OPENAI_MODEL_TTS_1,
	MENU_ITEM_SPEECH_OPENAI_MODEL_TTS_1_HD,
	MENU_ITEM_IMAGE_MODEL,
	MENU_ITEM_IMAGE_MODEL_DALL_E_2,
	MENU_ITEM_IMAGE_MODEL_DALL_E_3,
	MENU_ITEM_IMAGE_SIZE_DALL_E_2,
	MENU_ITEM_IMAGE_SIZE_DALL_E_2_256X256,
	MENU_ITEM_IMAGE_SIZE_DALL_E_2_512X512,
	MENU_ITEM_IMAGE_SIZE_DALL_E_2_1024X1024,
	MENU_ITEM_IMAGE_SIZE_DALL_E_3,
	MENU_ITEM_IMAGE_SIZE_DALL_E_3_1024X1024,
	MENU_ITEM_IMAGE_SIZE_DALL_E_3_1792X1024,
	MENU_ITEM_IMAGE_SIZE_DALL_E_3_1024X1792,
	MENU_ITEM_CHAT_SYSTEM
};

#ifdef __AMIGAOS4__
#define IntuitionBase Library
#define GfxBase Library
struct IntuitionIFace *IIntuition;
struct AslIFace *IAsl;
struct GraphicsIFace *IGraphics;
struct AmigaGuideIFace *IAmigaGuide;
extern struct UtilityIFace *IUtility;
struct DataTypesIFace *IDataTypes;
struct MUIMasterIFace *IMUIMaster;
struct GadToolsIFace *IGadTools;
#endif

struct IntuitionBase *IntuitionBase;
struct GfxBase *GfxBase;
struct Library *AmigaGuideBase;
struct Library *AslBase;
struct Library *GadToolsBase;
struct Library *DataTypesBase;
struct Library *MUIMasterBase;
struct Window *mainWindow;
struct Window *imageWindow;
static Object *mainWindowObject;
static Object *imageWindowObject;
static Object *aboutAmigaGPTWindowObject;
static Object *aboutMUIWindowObject = NULL;
static Object *mainGroup;
static Object *modeClickTab;
static Object *sendMessageButton;
static Object *chatInputTextEditor;
static Object *chatOutputTextEditor;
static Object *chatOutputScroller;
static Object *statusBar;
static Object *conversationListObject;
static Object *dataTypeObject;
static Object *app;
static struct Screen *screen;
static BOOL isPublicScreen;
static BOOL isAmigaOS3X;
static LONG selectedMode;
static WORD pens[NUMDRIPENS + 1];
static LONG textEditorColorMap[] = {5,10,6,3,6,6,4,0,1,6,6,6,6,6,6,6};
static LONG sendMessageButtonPen;
static LONG newChatButtonPen;
static LONG deleteButtonPen;
struct Conversation *currentConversation;
static struct GeneratedImage *currentImage;
struct List *modeSelectionTabList;
struct List *imageList;
static struct TextFont *uiTextFont = NULL;
static struct TextAttr screenFont = {
	.ta_Name = "",
	.ta_YSize = 8,
	.ta_Style = FS_NORMAL,
	.ta_Flags = FPF_DISKFONT | FPF_DESIGNED
};
static struct TextAttr chatTextAttr = {0};
static struct TextAttr uiTextAttr = {0};
static struct NewMenu amigaGPTMenu[] = {
	{NM_TITLE, "Project", 0, 0, 0, (APTR)NULL_ID},
	{NM_ITEM, "About AmigaGPT", 0, 0, 0, (APTR)MENU_ITEM_ABOUT_AMIGAGPT},
	{NM_ITEM, "About MUI", 0, 0, 0, (APTR)MENU_ITEM_ABOUT_MUI},
	{NM_ITEM, NM_BARLABEL, 0, 0, 0, (APTR)NULL_ID},
	{NM_ITEM, "Quit", "Q", 0, 0, (APTR)MENU_ITEM_QUIT},
	{NM_TITLE, "Edit", 0, 0, 0, (APTR)NULL_ID},
	{NM_ITEM, "Cut", "X", 0, 0, (APTR)MENU_ITEM_CUT},
	{NM_ITEM, "Copy", "C", 0, 0, (APTR)MENU_ITEM_COPY},
	{NM_ITEM, "Paste", "V", 0, 0, (APTR)MENU_ITEM_PASTE},
	{NM_ITEM, "Clear", "L", 0, 0, (APTR)MENU_ITEM_CLEAR},
	{NM_ITEM, "Select all", "A", 0, 0, (APTR)MENU_ITEM_SELECT_ALL},
	{NM_TITLE, "View", 0, 0, 0, (APTR)NULL_ID},
	{NM_ITEM, "Chat Font", 0, 0, 0, (APTR)MENU_ITEM_CHAT_FONT},
	{NM_ITEM, "UI Font", 0, 0, 0, (APTR)MENU_ITEM_UI_FONT},
	{NM_TITLE, "Speech", 0, 0, 0, (APTR)NULL_ID},
	{NM_ITEM, "Enabled", 0, CHECKIT|CHECKED, 0, (APTR)MENU_ITEM_SPEECH_ENABLED},
	{NM_ITEM, "Speech system", 0, 0, 0, (APTR)MENU_ITEM_SPEECH_SYSTEM},
	#ifdef __AMIGAOS3__
	{NM_SUB, "Workbench 1.x v34", 0, CHECKIT|CHECKED, 0, (APTR)MENU_ITEM_SPEECH_SYSTEM_34},
	{NM_SUB, "Workbench 2.0 v37", 0, CHECKIT, 0, (APTR)MENU_ITEM_SPEECH_SYSTEM_37},
	#else
	#ifdef __AMIGAOS4__
	{NM_SUB, "Flite", 0, CHECKIT|CHECKED, 0, (APTR)MENU_ITEM_SPEECH_SYSTEM_FLITE},
	#endif
	#endif
	{NM_SUB, "OpenAI Text To Speech", 0, CHECKIT, 0, (APTR)MENU_ITEM_SPEECH_SYSTEM_OPENAI},
	#ifdef __AMIGAOS3__
	{NM_ITEM, "Accent", 0, 0, 0, (APTR)MENU_ITEM_SPEECH_ACCENT},
	#endif
	#ifdef __AMIGAOS4__
	{NM_ITEM, "Flite Voice", 0, 0, 0, (APTR)MENU_ITEM_SPEECH_FLITE_VOICE},
	{NM_SUB, "kal (fast)", 0, CHECKIT, 0, (APTR)MENU_ITEM_SPEECH_FLITE_VOICE_KAL},
	{NM_SUB, "kal16 (fast)", 0, CHECKIT, 0, (APTR)MENU_ITEM_SPEECH_FLITE_VOICE_KAL16},
	{NM_SUB, "awb (slow)", 0, CHECKIT|CHECKED, 0, (APTR)MENU_ITEM_SPEECH_FLITE_VOICE_AWB},
	{NM_SUB, "rms (slow)", 0, CHECKIT, 0, (APTR)MENU_ITEM_SPEECH_FLITE_VOICE_RMS},
	{NM_SUB, "slt (slow)", 0, CHECKIT, 0, (APTR)MENU_ITEM_SPEECH_FLITE_VOICE_SLT},
	#endif
	{NM_ITEM, "OpenAI Voice", 0, 0, 0, (APTR)MENU_ITEM_SPEECH_SYSTEM_OPENAI},
	{NM_SUB, "alloy", 0, CHECKIT|CHECKED, 0, (APTR)MENU_ITEM_SPEECH_OPENAI_VOICE_ALLOY},
	{NM_SUB, "echo", 0, CHECKIT, 0, (APTR)MENU_ITEM_SPEECH_OPENAI_VOICE_ECHO},
	{NM_SUB, "fable", 0, CHECKIT, 0, (APTR)MENU_ITEM_SPEECH_OPENAI_VOICE_FABLE},
	{NM_SUB, "onyx", 0, CHECKIT, 0, (APTR)MENU_ITEM_SPEECH_OPENAI_VOICE_ONYX},
	{NM_SUB, "nova", 0, CHECKIT, 0, (APTR)MENU_ITEM_SPEECH_OPENAI_VOICE_NOVA},
	{NM_SUB, "shimmer", 0, CHECKIT, 0, (APTR)MENU_ITEM_SPEECH_OPENAI_VOICE_SHIMMER},
	{NM_ITEM, "OpenAI Speech Model", 0, 0, 0, (APTR)MENU_ITEM_SPEECH_OPENAI_MODEL},
	{NM_SUB, "tts-1", 0, CHECKIT|CHECKED, 0, (APTR)MENU_ITEM_SPEECH_OPENAI_MODEL_TTS_1},
	{NM_SUB, "tts-1-hd", 0, CHECKIT, 0, (APTR)MENU_ITEM_SPEECH_OPENAI_MODEL_TTS_1_HD},
	{NM_TITLE, "OpenAI", 0, 0, 0, (APTR)NULL_ID},
	{NM_ITEM, "API key", 0, 0, 0, (APTR)MENU_ITEM_OPENAI_API_KEY},
	{NM_ITEM, "Chat System", 0, 0, 0, (APTR)MENU_ITEM_CHAT_SYSTEM},
	{NM_ITEM, "Chat Model", 0, 0, 0, (APTR)MENU_ITEM_CHAT_MODEL},
	{NM_SUB, "gpt-4o", 0, CHECKIT, 0, (APTR)MENU_ITEM_CHAT_MODEL_GPT_4o},
	{NM_SUB, "gpt-4o-2024-05-13", 0, CHECKIT, 0, (APTR)MENU_ITEM_CHAT_MODEL_GPT_4o_2024_05_13},
	{NM_SUB, "gpt-4o-mini", 0, CHECKIT, 0, (APTR)MENU_ITEM_CHAT_MODEL_GPT_4o_MINI},
	{NM_SUB, "gpt-4o-mini-2024-07-18", 0, CHECKIT, 0, (APTR)MENU_ITEM_CHAT_MODEL_GPT_4o_MINI_2024_07_18},
	{NM_SUB, "gpt-4-turbo", 0, CHECKIT, 0, (APTR)MENU_ITEM_CHAT_MODEL_GPT_4_TURBO},
	{NM_SUB, "gpt-4-turbo-2024-04-09", 0, CHECKIT, 0, (APTR)MENU_ITEM_CHAT_MODEL_GPT_4_TURBO_2024_04_09},
	{NM_SUB, "gpt-4-turbo-preview", 0, CHECKIT, 0, (APTR)MENU_ITEM_CHAT_MODEL_GPT_4_TURBO_PREVIEW},
	{NM_SUB, "gpt-4-0125-preview", 0, CHECKIT, 0, (APTR)MENU_ITEM_CHAT_MODEL_GPT_4_0125_PREVIEW},
	{NM_SUB, "gpt-4-1106-preview", 0, CHECKIT, 0, (APTR)MENU_ITEM_CHAT_MODEL_GPT_4_1106_PREVIEW},
	{NM_SUB, "gpt-4", 0, CHECKIT, 0, (APTR)MENU_ITEM_CHAT_MODEL_GPT_4},
	{NM_SUB, "gpt-4-0613", 0, CHECKIT, 0, (APTR)MENU_ITEM_CHAT_MODEL_GPT_4_0613},
	{NM_SUB, "gpt-3.5-turbo", 0, CHECKIT|CHECKED, 0, (APTR)MENU_ITEM_CHAT_MODEL_GPT_3_5_TURBO},
	{NM_SUB, "gpt-3.5-turbo-0125", 0, CHECKIT, 0, (APTR)MENU_ITEM_CHAT_MODEL_GPT_3_5_TURBO_0125},
	{NM_SUB, "gpt-3.5-turbo-1106", 0, CHECKIT, 0, (APTR)MENU_ITEM_CHAT_MODEL_GPT_3_5_TURBO_1106},
	{NM_ITEM, "Image Model", 0, 0, 0, (APTR)MENU_ITEM_IMAGE_MODEL},
	{NM_SUB, "dall-e-2", 0, CHECKIT, 0, (APTR)MENU_ITEM_IMAGE_MODEL_DALL_E_2},
	{NM_SUB, "dall-e-3", 0, CHECKIT|CHECKED, 0, (APTR)MENU_ITEM_IMAGE_MODEL_DALL_E_3},
	{NM_ITEM, "DALL-E 2 Image Size", 0, 0, 0, (APTR)MENU_ITEM_IMAGE_SIZE_DALL_E_2},
	{NM_SUB, "256x256", 0, CHECKIT|CHECKED, 0, (APTR)MENU_ITEM_IMAGE_SIZE_DALL_E_2_256X256},
	{NM_SUB, "512x512", 0, CHECKIT, 0, (APTR)MENU_ITEM_IMAGE_SIZE_DALL_E_2_512X512},
	{NM_SUB, "1024x1024", 0, CHECKIT, 0, (APTR)MENU_ITEM_IMAGE_SIZE_DALL_E_2_1024X1024},
	{NM_ITEM, "DALL-E 3 Image Size", 0, 0, 0, (APTR)MENU_ITEM_IMAGE_SIZE_DALL_E_3},
	{NM_SUB, "1024x1024", 0, CHECKIT|CHECKED, 0, (APTR)MENU_ITEM_IMAGE_SIZE_DALL_E_3_1024X1024},
	{NM_SUB, "1792x1024", 0, CHECKIT, 0, (APTR)MENU_ITEM_IMAGE_SIZE_DALL_E_3_1792X1024},
	{NM_SUB, "1024x1792", 0, CHECKIT, 0, (APTR)MENU_ITEM_IMAGE_SIZE_DALL_E_3_1024X1792},
	{NM_TITLE, "Help", 0, 0, 0, (APTR)NULL_ID},
	{NM_ITEM, "View Documentation", 0, 0, 0, (APTR)MENU_ITEM_VIEW_DOCUMENTATION},
	{NM_END, NULL, 0, 0, 0, 0}
};
/**
 * Struct representing a generated image
 * @see enum ImageSize
 * @see enum ImageModel
 **/ 
static struct GeneratedImage {
	STRPTR name;
	STRPTR filePath;
	STRPTR prompt;
	enum ImageModel imageModel;
	WORD width;
	WORD height;
};
struct MsgPort *appPort;
ULONG activeTextEditorGadgetID;
static struct Node *chatTabNode;
static struct Node *imageGenerationTabNode;

static STRPTR getMessageContentFromJson(struct json_object *json, BOOL stream);
static void formatText(STRPTR unformattedText);
static void sendChatMessage();
static void closeGUILibraries();
static LONG openStartupOptions();
static void refreshOpenAIMenuItems();
static void refreshSpeechMenuItems();
static struct Conversation* newConversation();
static struct Conversation* copyConversation(struct Conversation *conversation);
static void addTextToConversation(struct Conversation *conversation, STRPTR text, STRPTR role);
static void addImageToImageList(struct GeneratedImage *image);
static void displayConversation(struct Conversation *conversation);
static void freeConversation(struct Conversation *conversation);
static void freeConversationList();
static void freeImageList();
static void freeModeSelectionTabList();
static void removeConversationFromConversationList(struct Conversation *conversation);
static void saveImageCopy(struct GeneratedImage *image);
static void removeImageFromImageList(struct GeneratedImage *image);
static void openChatFontRequester();
static void openUIFontRequester();
static void openAboutAmigaGPTWindow();
static void openAboutMUIWindow();
static void openSpeechAccentRequester();
static void openApiKeyRequester();
static void openChatSystemRequester();
static void openImage(struct GeneratedImage *generatedImage, WORD width, WORD height);
static LONG loadConversations();
static LONG loadImages();
static LONG saveImages();
static LONG saveConversations();
static STRPTR ISO8859_1ToUTF8(CONST_STRPTR iso8859_1String);
static STRPTR UTF8ToISO8859_1(CONST_STRPTR utf8String);
static BOOL copyFile(STRPTR source, STRPTR destination);
static void openDocumentation();
static void updateMenu();
static void createImage();
// #ifdef __AMIGAOS4__
// static uint32 processIDCMPMainWindow(struct Hook *hook, struct Window *window, struct IntuiMessage *message);
// static uint32 processIDCMPCreateImageWindow(struct Hook *hook, struct Window *window, struct IntuiMessage *message);
// #else
// static void __SAVE_DS__ __ASM__ processIDCMP(__REG__ (a0, struct Hook *hook), __REG__ (a2, struct Window *window), __REG__ (a1, struct IntuiMessage *message));
// static void __SAVE_DS__ __ASM__ processIDCMPMainWindow(__REG__ (a0, struct Hook *hook), __REG__ (a2, struct Window *window), __REG__ (a1, struct IntuiMessage *message));
// #endif

// #ifdef __AMIGAOS4__
// static uint32 processIDCMPMainWindow(struct Hook *hook, struct Window *window, struct IntuiMessage *message) {
// #else
// static void __SAVE_DS__ __ASM__ processIDCMPMainWindow(__REG__ (a0, struct Hook *hook), __REG__ (a2, struct Window *window), __REG__ (a1, struct IntuiMessage *message)) {
// #endif
// 	switch (message->Class) {
// 		case IDCMP_IDCMPUPDATE:
// 		{
// 			struct TagItem *tagList = message->IAddress;
// 			struct TagItem *gadgetID = FindTagItem(GA_ID, tagList);
// 			switch (gadgetID->ti_Data) {
// 				case TEXT_INPUT_TEXT_EDITOR_ID:
// 					activeTextEditorGadgetID = TEXT_INPUT_TEXT_EDITOR_ID;
// 					break;					
// 				case CHAT_OUTPUT_SCROLLER_ID:
// 					gadgetID->ti_Tag = TAG_IGNORE;
// 					ULONG top;
// 					GetAttr(SCROLLER_Top, chatOutputScroller, &top);
// 					SetGadgetAttrs(chatOutputTextEditor, mainWindow, NULL,
// 					GA_TEXTEDITOR_Prop_First, top,
// 					TAG_DONE);
// 					break;
// 				case CHAT_OUTPUT_TEXT_EDITOR_ID:
// 					activeTextEditorGadgetID = CHAT_OUTPUT_TEXT_EDITOR_ID;
// 					gadgetID->ti_Tag = TAG_IGNORE;
// 					ULONG entries;
// 					GetAttr(GA_TEXTEDITOR_Prop_Entries, chatOutputTextEditor, &entries);
// 					ULONG first;
// 					GetAttr(GA_TEXTEDITOR_Prop_First, chatOutputTextEditor, &first);
// 					ULONG deltaFactor;
// 					GetAttr(GA_TEXTEDITOR_Prop_DeltaFactor, chatOutputTextEditor, &deltaFactor);
// 					ULONG visible;
// 					GetAttr(GA_TEXTEDITOR_Prop_Visible, chatOutputTextEditor, &visible);
// 					SetGadgetAttrs(chatOutputScroller, mainWindow, NULL,
// 					SCROLLER_Total, entries,
// 					SCROLLER_Top, first,
// 					SCROLLER_ArrowDelta, deltaFactor,
// 					SCROLLER_Visible, visible,
// 					TAG_DONE);
// 					RefreshGadgets(chatOutputScroller, mainWindow, NULL);
// 					break;
// 				}
// 			break;
// 		}
// 		default:
// 			printf("Unknown message class: %lx\n", message->Class);
// 			break;
// 	}
// 	#ifdef __AMIGAOS3__
// 	return;
// 	#else
// 	return WHOOKRSLT_IGNORE;
// 	#endif
// }

// #ifdef __AMIGAOS4__
// static uint32 processIDCMPCreateImageWindow(struct Hook *hook, struct Window *window, struct IntuiMessage *message) {
// #else
// static void __SAVE_DS__ __ASM__ processIDCMPCreateImageWindow(__REG__ (a0, struct Hook *hook), __REG__ (a2, struct Window *window), __REG__ (a1, struct IntuiMessage *message)) {
// #endif
// 	switch (message->Class) {
// 		case IDCMP_IDCMPUPDATE:
// 		{
// 			UWORD MsgCode = message->Code;
// 			struct TagItem *MsgTags = message->IAddress;
// 			struct TagItem *List = MsgTags;
// 			struct TagItem *This;

// 			while((This = NextTagItem(&List)) != NULL)
// 			{
// 				switch(This->ti_Tag)
// 				{
// 					case DTA_Busy:
// 						if(This->ti_Data)
// 						{
// 							SetWindowPointer(imageWindow,
// 								WA_BusyPointer,	TRUE,
// 							TAG_DONE);
// 							updateStatusBar("Processing image...", 7);
// 						}
// 						else
// 						{
// 							SetWindowPointerA(imageWindow,NULL);
// 							updateStatusBar("Ready", 5);
// 						}

// 						break;

// 					case DTA_Sync:
// 						SetAttrs(dataTypeObject,
// 						 GA_RelWidth, imageWindow->Width - imageWindow->BorderLeft - imageWindow->BorderRight,
// 						GA_RelHeight, imageWindow->Height - imageWindow->BorderTop - imageWindow->BorderBottom,
// 						 TAG_DONE);
// 						RefreshDTObjects(dataTypeObject,imageWindow,NULL,NULL);
// 						break;
// 				}
// 			}
// 			break;
// 		}
// 		case IDCMP_REFRESHWINDOW:
// 			BeginRefresh(imageWindow);
// 			EndRefresh(imageWindow,TRUE);
// 			break;
// 		default:
// 			printf("Unknown message class: %lx\n", message->Class);
// 			break;
// 	}
// 	#ifdef __AMIGAOS3__
// 	return;
// 	#else
// 	return WHOOKRSLT_IGNORE;
// 	#endif
// }

/**
 * Open the libraries needed for the GUI
 * @return RETURN_OK on success, RETURN_ERROR on failure
**/
LONG openGUILibraries() {
	#if defined(__AMIGAOS3__) || defined(__MORPHOS__)
	if ((IntuitionBase = (struct IntuitionBase *)OpenLibrary("intuition.library", 40)) == NULL) {
		printf("Could not open intuition.library\n");
		return RETURN_ERROR;
	}
	#else
	if ((IntuitionBase = OpenLibrary("intuition.library", 50)) == NULL) {
		printf("Could not open intuition.library\n");
		return RETURN_ERROR;
	}
	if ((IIntuition = (struct IntuitionIFace *)GetInterface(IntuitionBase, "main", 1, NULL)) == NULL) {
		printf("Could not get interface for intuition.library\n");
		return RETURN_ERROR;
	}
	#endif

	#if defined(__AMIGAOS3__) || defined(__MORPHOS__)
	if ((MUIMasterBase = OpenLibrary("muimaster.library", 19)) == NULL) {
		printf("Could not open muimaster.library\n");
		return RETURN_ERROR;
	}
	#else
	if ((MUIMasterBase = OpenLibrary("muimaster.library", 19)) == NULL) {
		printf("Could not open muimaster.library\n");
		return RETURN_ERROR;
	}
	if ((IMUIMaster = (struct MUIIFace *)GetInterface(MUIMasterBase, "main", 1, NULL)) == NULL) {
		printf("Could not get interface for muimaster.library\n");
		return RETURN_ERROR;
	}
	#endif

	#if defined(__AMIGAOS3__) || defined(__MORPHOS__)
	if ((GfxBase = (struct GfxBase *)OpenLibrary("graphics.library", 40)) == NULL) {
		printf( "Could not open graphics.library\n");
		return RETURN_ERROR;
	}
	#else
	if ((GfxBase = OpenLibrary("graphics.library", 50)) == NULL) {
		printf( "Could not open graphics.library\n");
		return RETURN_ERROR;
	}
	if ((IGraphics = (struct GraphicsIFace *)GetInterface(GfxBase, "main", 1, NULL)) == NULL) {
		printf( "Could not get interface for graphics.library\n");
		return RETURN_ERROR;
	}
	#endif

	#if defined(__AMIGAOS3__) || defined(__MORPHOS__)
	if ((AslBase = OpenLibrary("asl.library", 45)) == NULL) {
		printf( "Could not open asl.library\n");
		return RETURN_ERROR;
	}
	#else
	if ((AslBase = OpenLibrary("asl.library", 50)) == NULL) {
		printf( "Could not open asl.library\n");
		return RETURN_ERROR;
	}
	if ((IAsl = (struct AslIFace *)GetInterface(AslBase, "main", 1, NULL)) == NULL) {
		printf( "Could not get interface for asl.library\n");
		return RETURN_ERROR;
	}
	#endif

	#if defined(__AMIGAOS3__) || defined(__MORPHOS__)
	if ((AmigaGuideBase = OpenLibrary("amigaguide.library", 44)) == NULL) {
		printf( "Could not open amigaguide.library\n");
		return RETURN_ERROR;
	}
	#else
	if ((AmigaGuideBase = OpenLibrary("amigaguide.library", 50)) == NULL) {
		printf( "Could not open amigaguide.library\n");
		return RETURN_ERROR;
	}
	if ((IAmigaGuide = (struct AmigaGuideIFace *)GetInterface(AmigaGuideBase, "main", 1, NULL)) == NULL) {
		printf( "Could not get interface for amigaguide.library\n");
		return RETURN_ERROR;
	}
	#endif

	#if defined(__AMIGAOS3__) || defined(__MORPHOS__)
	if ((GadToolsBase = OpenLibrary("gadtools.library", 40)) == NULL) {
		printf( "Could not open gadtools.library\n");
		return RETURN_ERROR;
	}
	#else
	if ((GadToolsBase = OpenLibrary("gadtools.library", 50)) == NULL) {
		printf( "Could not open gadtools.library\n");
		return RETURN_ERROR;
	}
	if ((IGadTools = (struct GadToolsIFace *)GetInterface(GadToolsBase, "main", 1, NULL)) == NULL) {
		printf( "Could not get interface for gadtools.library\n");
		return RETURN_ERROR;
	}
	#endif

	#if defined(__AMIGAOS3__) || defined(__MORPHOS__)
	if ((DataTypesBase = OpenLibrary("datatypes.library", 44)) == NULL) {
		printf( "Could not open datatypes.library\n");
		return RETURN_ERROR;
	}
	#else
	if ((DataTypesBase = OpenLibrary("datatypes.library", 50)) == NULL) {
		printf( "Could not open datatypes.library\n");
		return RETURN_ERROR;
	}
	if ((IDataTypes = (struct DataTypesIFace *)GetInterface(DataTypesBase, "main", 1, NULL)) == NULL) {
		printf( "Could not get interface for datatypes.library\n");
		return RETURN_ERROR;
	}
	#endif

	return RETURN_OK;
}

HOOKPROTONHNO(ConstructLI_TextFunc, APTR, struct NList_ConstructMessage *ncm) {
	struct Conversation *oldEntry = (struct Conversation *)ncm->entry;
	struct Conversation *newEntry = copyConversation(oldEntry);
    return (newEntry);
}
MakeHook(ConstructLI_TextHook, ConstructLI_TextFunc);

HOOKPROTONHNO(DestructLI_TextFunc, void, struct NList_DestructMessage *ndm) {
	if (ndm->entry)
		freeConversation((struct Conversation *)ndm->entry);
}
MakeHook(DestructLI_TextHook, DestructLI_TextFunc);

HOOKPROTONHNO(DisplayLI_TextFunc, void, struct NList_DisplayMessage *ndm) {
  struct Conversation *entry = (struct Conversation *) ndm->entry;
  ndm->strings[0] = (STRPTR)entry->name;
}
MakeHook(DisplayLI_TextHook, DisplayLI_TextFunc);

HOOKPROTONHNO(ConversationRowClickedFunc, void, LONG *rowNumber) {
	if (currentConversation != NULL)
		freeConversation(currentConversation);

	struct Conversation *conversation;
	DoMethod(conversationListObject, MUIM_NList_GetEntry, MUIV_NList_GetEntry_Active, &conversation);
	currentConversation = copyConversation(conversation);
	displayConversation(currentConversation);
}
MakeHook(ConversationRowClickedHook, ConversationRowClickedFunc);

/**
 * Close the libraries used by the GUI
**/
static void closeGUILibraries() {
	#ifdef __AMIGAOS4__
	DropInterface((struct Interface *)IIntuition);
	DropInterface((struct Interface *)IGraphics);
	DropInterface((struct Interface *)IAsl);
	DropInterface((struct Interface *)IAmigaGuide);
	DropInterface((struct Interface *)IDataTypes);
	DropInterface((struct Interface *)IMUIMaster);
	DropInterface((struct Interface *)IGadTools);
	#endif

	CloseLibrary(GadToolsBase);
	CloseLibrary(IntuitionBase);
	CloseLibrary(GfxBase);
	CloseLibrary(AslBase);
	CloseLibrary(AmigaGuideBase);
	CloseLibrary(DataTypesBase);
	CloseLibrary(MUIMasterBase);
}

/**
 * Create the GUI
 * @return RETURN_OK on success, RETURN_ERROR on failure
**/
LONG initVideo() {
	if (openGUILibraries() == RETURN_ERROR) {
		return RETURN_ERROR;
	}

	if (!(app = ApplicationObject,
		MUIA_Application_Title, "AmigaGPT",
		MUIA_Application_Version, APP_VERSION,
		MUIA_Application_Copyright, "(C) 2023-2024 Cameron Armstrong (Nightfox/sacredbanana)",
		MUIA_Application_Author, "Cameron Armstrong (Nightfox/sacredbanana)",
		MUIA_Application_Description, "AmigaGPT is an app for chatting to ChatGPT or creating AI images with DALL-E",
		End)) {
		printf("Could not create app!\n");
		return RETURN_ERROR;
	}

	if ((aboutAmigaGPTWindowObject = AboutboxObject,
				MUIA_Aboutbox_Build, BUILD_NUMBER " ("__DATE__ ")\n""Git commit: " GIT_COMMIT "\nGit branch: " GIT_BRANCH "\n" "Commit timestamp: " GIT_TIMESTAMP,
				MUIA_Aboutbox_Credits, "This app will always remain free but if you would like to support me you can do so at https://paypal.me/sacredbanana\n"
									"\n"
									"Click the version string above for build details.\n"
									"\n"
									"\033b%p\033n\n"
									"\t\033iCameron Armstrong\033n\n"
									"\t(@sacredbanana on GitHub, YouTube and Twitter, @Nightfox on EAB)\n"
									"\n"
									"\033b%I\033n\n"
									"\t\033iMauricio Sandoval\033n\n"
									"\n"
									"\033b%T\033n\n"
									"\t\033iBebbo\033n\n"
									"\tfor creating the Amiga GCC toolchain\n"
									"\thttps://github.com/bebbo\n"
									"\n"
									"\t\033iOpenAI\033n\n"
									"\tfor creating the GPT and DALL-E models\n"
									"\n"
									"\t\033iEAB\033n\n"
									"\tfor being a great community!\n"	
									"\n"
									"\t\033iJan Zahurancik\033n\n"
									"\tfor all the thorough testing, bundling AmigaGPT into AmiKit and for all the moral support\n"
									"\thttps://www.amikit.amiga.sk\n"
									"\n"
									"\t\033iCoffinOS\033n\n"
									"\tfor bundling AmigaGPT into CoffinOS\n"
									"\thttps://getcoffin.net\n"
									"\n"
									"\t\033iAmiga Future Magazine\033n\n"
									"\tfor reviewing AmigaGPT and publishing several of its updates in the News from Aminet section\n"
									"\thttps://www.amigafuture.de\n"
									"\n"
									"\t\033iWhatIFF? Magazine\033n\n"
									"\tfor reviewing AmigaGPT and interviewing me in issue 14\n"
									"\thttps://www.whatiff.info\n"
									"\n"
									"\t\033iDan Wood\033n\n"
									"\tfor reviewing AmigaGPT on his YouTube channel\n"
									"\thttps://www.youtube.com/watch?v=-OA28r8Up5U\n"
									"\n"
									"\t\033iProteque-CBN\033n\n"
									"\tfor reviewing AmigaGPT on his YouTube channel\n"
									"\thttps://www.youtube.com/watch?v=t3q8HQ6wrnw\n"
									"\n"
									"\t\033iAmigaBill\033n\n"
									"\tfor covering AmigaGPT in the Amiga News section on his Twitch streams and allowing me to join his stream to promote it\n"
									"\thttps://www.twitch.tv/amigabill\n"
									"\n"
									"\t\033iLes Docs\033n\n"
									"\tfor making a video review and giving a tutorial on how to add support for the French accent\n"
									"\thttps://www.youtube.com/watch?v=BV5Fq1PresE\n"
									"\n"
									"\033bLicense\033n\n"
									"\tMIT\n"
									"\n"
									"\033b%W\033n\n"
									"\thttps://github.com/sacredbanana/AmigaGPT/issues\n"
									"\thttps://eab.abime.net/showthread.php?t=114798\n",
				MUIA_Aboutbox_URL, "https://github.com/sacredbanana/AmigaGPT",
				MUIA_Aboutbox_URLText, "Visit the GitHub repository for the latest release",
			End) ) {
				DoMethod(app, OM_ADDMEMBER, aboutAmigaGPTWindowObject);
				DoMethod(aboutAmigaGPTWindowObject, MUIM_Notify, MUIA_Window_CloseRequest, TRUE, MUIV_Notify_Self, 3, MUIM_Set, MUIA_Window_Open, FALSE);
	} else {
		printf("Warning: Could not create aboutAmigaGPTWindowObject\nThe installed MUI version is probably too old.\nFalling back to a simple requester.\n");
	}

	if (openStartupOptions() == RETURN_ERROR)
		return RETURN_ERROR;

	set(aboutAmigaGPTWindowObject, MUIA_Window_Screen, screen);

	// modeSelectionTabList = AllocVec(sizeof(struct List), MEMF_CLEAR);
	// NewList(modeSelectionTabList);
	// chatTabNode = AllocClickTabNode(TAG_DONE);
	// SetClickTabNodeAttrs(chatTabNode,
	//  TNA_Number, MODE_SELECTION_TAB_CHAT_ID,
	//  TNA_Text, "Chat",
	//  TNA_TextPen, isPublicScreen ? ObtainBestPen(screen->ViewPort.ColorMap, 0x00000000, 0x00000000, 0x00000000, OBP_Precision, PRECISION_GUI, TAG_DONE) : 1,
	//  #ifdef __AMIGAOS4__
	//  TNA_HintInfo, "Have a text conversation with ChatGPT",
	//  #endif
	//   TAG_DONE);
	// imageGenerationTabNode = AllocClickTabNode(TAG_DONE);
	// SetClickTabNodeAttrs(imageGenerationTabNode,
	//  TNA_Number, MODE_SELECTION_TAB_IMAGE_GENERATION_ID,
	//  TNA_Text, "Image Generation",
	//  TNA_TextPen, isPublicScreen ? ObtainBestPen(screen->ViewPort.ColorMap, 0x00000000, 0x00000000, 0x00000000, OBP_Precision, PRECISION_GUI, TAG_DONE) : 1,
	//  #ifdef __AMIGAOS4__
	//  TNA_HintInfo, "Generate an image from a text prompt",
	//  #endif
	//   TAG_DONE);
	// AddTail(modeSelectionTabList, chatTabNode);	
	// AddTail(modeSelectionTabList, imageGenerationTabNode);

	// conversationList = AllocVec(sizeof(struct List), MEMF_CLEAR);
	// NewList(conversationList);
	currentConversation = NULL;
	// loadConversations();

	// imageList = AllocVec(sizeof(struct List), MEMF_CLEAR);
	// NewList(imageList);
	// currentImage = NULL;
	// loadImages();

	uiTextAttr.ta_Name = config.uiFontName ? config.uiFontName : "";
	uiTextAttr.ta_YSize = config.uiFontSize;
	uiTextAttr.ta_Style = config.uiFontStyle;
	uiTextAttr.ta_Flags = config.uiFontFlags;
	chatTextAttr.ta_Name = config.chatFontName ? config.chatFontName : "";
	chatTextAttr.ta_YSize = config.chatFontSize;
	chatTextAttr.ta_Style = config.chatFontStyle;
	chatTextAttr.ta_Flags = config.chatFontFlags;

	ULONG pen = 8;
	sendMessageButtonPen = isPublicScreen ? ObtainBestPen(screen->ViewPort.ColorMap, 0x00000000, 0x00000000, 0xFFFFFFFF, OBP_Precision, PRECISION_GUI, TAG_DONE) : 1;

	if (!(mainWindowObject = WindowObject,
		MUIA_Window_Title, "AmigaGPT",
		MUIA_Window_ID, MAIN_WINDOW_ID,
		MUIA_Window_CloseGadget, TRUE,
		MUIA_Window_DepthGadget, isPublicScreen,
		MUIA_Window_SizeGadget, isPublicScreen,
		MUIA_Window_DragBar, isPublicScreen,
		MUIA_Window_Screen, screen,
		MUIA_Window_Width, MUIV_Window_Width_Visible(isPublicScreen ? 90: 100),
		MUIA_Window_Height, MUIV_Window_Height_Visible(isPublicScreen ? 90: 100),
		MUIA_Window_LeftEdge, MUIV_Window_LeftEdge_Centered,
		MUIA_Window_TopEdge, MUIV_Window_TopEdge_Centered,
		MUIA_Window_Menustrip, MUI_MakeObject(MUIO_MenustripNM, amigaGPTMenu),
		MUIA_Window_SizeRight, TRUE,
		MUIA_Window_UseBottomBorderScroller, FALSE,
		MUIA_Window_UseRightBorderScroller, FALSE,
		MUIA_Window_UseLeftBorderScroller, FALSE,
		WindowContents, HGroup,
			Child, VGroup,
				Child, NListviewObject,
					MUIA_CycleChain, 1,

					MUIA_NListview_NList, conversationListObject = NListObject,
						MUIA_NList_DefaultObjectOnClick, TRUE,
						MUIA_NList_MultiSelect, MUIV_NList_MultiSelect_None,
						MUIA_NList_ConstructHook2, &ConstructLI_TextHook,
						MUIA_NList_DestructHook2, &DestructLI_TextHook,
						MUIA_NList_DisplayHook2, &DisplayLI_TextHook,
						MUIA_NList_Format, "BAR MINW=100 MAXW=200",
						MUIA_NList_AutoVisible, TRUE,
						MUIA_NList_TitleSeparator, FALSE,
						MUIA_NList_Title, FALSE,
						// MUIA_NList_EntryValueDependent, TRUE,
						MUIA_NList_MinColSortable, 0,
						MUIA_NList_Imports, MUIV_NList_Imports_All,
						MUIA_NList_Exports, MUIV_NList_Exports_All,
					End,
				End,
			End,
			Child, VGroup,
				// Chat output text display
				Child, HGroup,
					Child, chatOutputTextEditor = TextEditorObject,
						MUIA_TextEditor_Contents, "",
						MUIA_Text_Copy, TRUE,
						MUIA_Text_Marking, TRUE,
						MUIA_Text_SetMin, FALSE,
						MUIA_Text_SetMax, FALSE,
						MUIA_Text_SetVMax, FALSE,
						MUIA_Text_Shorten, MUIV_Text_Shorten_Nothing,
						MUIA_TextEditor_ReadOnly, TRUE,
						MUIA_TextEditor_Slider, chatOutputScroller,
					End,
					Child, chatOutputScroller = ScrollbarObject,
					End,
				End,
				// Status bar
				Child, StringObject,
					MUIA_String_Contents, "Ready",
					MUIA_String_MaxLen, 1024,
					MUIA_CycleChain, TRUE,
					MUIA_String_Accept, TRUE,
					MUIA_String_AdvanceOnCR, TRUE,
					MUIA_Background, MUII_FILL,
				End,
				Child, HGroup,
				// Chat input text editor
					Child, chatInputTextEditor = TextEditorObject,
						MUIA_TextEditor_Contents, "",
						MUIA_TextEditor_ReadOnly, FALSE,
						MUIA_TextEditor_TabSize, 4,
						MUIA_TextEditor_Pen, pen,
					End,
					// Send message button
					Child, sendMessageButton = MUI_MakeObject(MUIO_Button, "Send",
						MUIA_Width, 500,
						MUIA_Background, MUII_FILL,
						MUIA_Text_Contents, "Send",
						MUIA_CycleChain, TRUE,
						MUIA_InputMode, MUIV_InputMode_RelVerify,
					End,
				End,
			End,
		End,
	End)) {
		printf("Could not create mainWindowObject\n");
		return RETURN_ERROR;
	}

	DoMethod(conversationListObject, MUIM_Notify, MUIA_NList_EntryClick, MUIV_EveryTime, app, 3, MUIM_CallHook, &ConversationRowClickedHook, MUIV_TriggerValue);
	DoMethod(mainWindowObject, MUIM_Notify, MUIA_Window_CloseRequest, TRUE, 
	  app, 2, MUIM_Application_ReturnID, MUIV_Application_ReturnID_Quit);
	DoMethod(app, OM_ADDMEMBER, mainWindowObject);

	set(mainWindowObject,MUIA_Window_Open,TRUE);

	loadConversations();
	
	// if ((sendMessageButton = NewObject(BUTTON_GetClass(), NULL,
	// 	GA_ID, SEND_MESSAGE_BUTTON_ID,
	// 	BUTTON_TextPen, sendMessageButtonPen,
	// 	#ifdef __AMIGAOS3__
	// 	BUTTON_BackgroundPen, 0,
	// 	#endif
	// 	GA_TextAttr, &uiTextAttr,
	// 	BUTTON_Justification, BCJ_CENTER,
	// 	GA_Text, (ULONG)"Send",
	// 	GA_RelVerify, TRUE,
	// 	ICA_TARGET, ICTARGET_IDCMP,
	// 	TAG_DONE)) == NULL) {
	// 		printf("Could not create send message button\n");
	// 		return RETURN_ERROR;
	// }

	// if ((createImageButton = NewObject(BUTTON_GetClass(), NULL,
	// 	GA_ID, CREATE_IMAGE_BUTTON_ID,
	// 	BUTTON_TextPen, sendMessageButtonPen,
	// 	#ifdef __AMIGAOS3__
	// 	BUTTON_BackgroundPen, 0,
	// 	#endif
	// 	GA_TextAttr, &uiTextAttr,
	// 	BUTTON_Justification, BCJ_CENTER,
	// 	GA_Text, (ULONG)"Create Image",
	// 	GA_RelVerify, TRUE,
	// 	ICA_TARGET, ICTARGET_IDCMP,
	// 	TAG_DONE)) == NULL) {
	// 		printf("Could not create create image button\n");
	// 		return RETURN_ERROR;
	// }

	// if ((imageListBrowser = NewObject(LISTBROWSER_GetClass(), NULL,
	// 	GA_ID, IMAGE_LIST_BROWSER_ID,
	// 	GA_RelVerify, TRUE,
	// 	GA_TextAttr, &uiTextAttr,
	// 	LISTBROWSER_WrapText, TRUE,
	// 	LISTBROWSER_AutoFit, TRUE,
	// 	LISTBROWSER_ShowSelected, TRUE,
	// 	LISTBROWSER_Labels, imageList,
	// 	TAG_DONE)) == NULL) {
	// 		printf("Could not create image list browser\n");
	// 		return RETURN_ERROR;
	// }

	// pen = 5;
	// newChatButtonPen = isPublicScreen ? ObtainBestPen(screen->ViewPort.ColorMap, config.colors[pen*3+1], config.colors[pen*3+2], config.colors[pen*3+3], OBP_Precision, PRECISION_GUI, TAG_DONE) : pen;

	// if ((newImageButton = NewObject(BUTTON_GetClass(), NULL,
	// 	GA_ID, NEW_IMAGE_BUTTON_ID,
	// 	BUTTON_TextPen, newChatButtonPen,
	// 	#ifdef __AMIGAOS3__
	// 	BUTTON_BackgroundPen, 0,
	// 	#endif
	// 	GA_TextAttr, &uiTextAttr,
	// 	BUTTON_Justification, BCJ_CENTER,
	// 	GA_Text, (ULONG)"+ New Image",
	// 	GA_RelVerify, TRUE,
	// 	ICA_TARGET, ICTARGET_IDCMP,
	// 	TAG_DONE)) == NULL) {
	// 		printf("Could not create new image button\n");
	// 		return RETURN_ERROR;
	// }

	// pen = 6;
	// deleteButtonPen = isPublicScreen ? ObtainBestPen(screen->ViewPort.ColorMap, config.colors[pen*3+1], config.colors[pen*3+2], config.colors[pen*3+3], OBP_Precision, PRECISION_GUI, TAG_DONE) : pen;

	// if ((deleteImageButton = NewObject(BUTTON_GetClass(), NULL,
	// 	GA_ID, DELETE_IMAGE_BUTTON_ID,
	// 	BUTTON_TextPen, deleteButtonPen,
	// 	#ifdef __AMIGAOS3__
	// 	BUTTON_BackgroundPen, 0,
	// 	#endif
	// 	GA_TextAttr, &uiTextAttr,
	// 	BUTTON_Justification, BCJ_CENTER,
	// 	GA_Text, (ULONG)"- Delete Image",
	// 	GA_RelVerify, TRUE,
	// 	ICA_TARGET, ICTARGET_IDCMP,
	// 	TAG_DONE)) == NULL) {
	// 		printf("Could not create delete image button\n");
	// 		return RETURN_ERROR;
	// }

	// if ((openSmallImageButton = NewObject(BUTTON_GetClass(), NULL,
	// 	GA_ID, OPEN_SMALL_IMAGE_BUTTON_ID,
	// 	GA_Disabled, TRUE,
	// 	BUTTON_TextPen, sendMessageButtonPen,
	// 	#ifdef __AMIGAOS3__
	// 	BUTTON_BackgroundPen, 0,
	// 	#endif
	// 	GA_TextAttr, &uiTextAttr,
	// 	BUTTON_Justification, BCJ_CENTER,
	// 	GA_Text, (ULONG)"Open Small Image",
	// 	GA_RelVerify, TRUE,
	// 	ICA_TARGET, ICTARGET_IDCMP,
	// 	TAG_DONE)) == NULL) {
	// 		printf("Could not create open small image button\n");
	// 		return RETURN_ERROR;
	// }

	// if ((openMediumImageButton = NewObject(BUTTON_GetClass(), NULL,
	// 	GA_ID, OPEN_MEDIUM_IMAGE_BUTTON_ID,
	// 	GA_Disabled, TRUE,
	// 	BUTTON_TextPen, sendMessageButtonPen,
	// 	#ifdef __AMIGAOS3__
	// 	BUTTON_BackgroundPen, 0,
	// 	#endif
	// 	GA_TextAttr, &uiTextAttr,
	// 	BUTTON_Justification, BCJ_CENTER,
	// 	GA_Text, (ULONG)"Open Medium Image",
	// 	GA_RelVerify, TRUE,
	// 	ICA_TARGET, ICTARGET_IDCMP,
	// 	TAG_DONE)) == NULL) {
	// 		printf("Could not create open medium image button\n");
	// 		return RETURN_ERROR;
	// }

	// if ((openLargeImageButton = NewObject(BUTTON_GetClass(), NULL,
	// 	GA_ID, OPEN_LARGE_IMAGE_BUTTON_ID,
	// 	GA_Disabled, TRUE,
	// 	BUTTON_TextPen, sendMessageButtonPen,
	// 	#ifdef __AMIGAOS3__
	// 	BUTTON_BackgroundPen, 0,
	// 	#endif
	// 	GA_TextAttr, &uiTextAttr,
	// 	BUTTON_Justification, BCJ_CENTER,
	// 	GA_Text, (ULONG)"Open Large Image",
	// 	GA_RelVerify, TRUE,
	// 	ICA_TARGET, ICTARGET_IDCMP,
	// 	TAG_DONE)) == NULL) {
	// 		printf("Could not create open large image button\n");
	// 		return RETURN_ERROR;
	// }

	// if ((openOriginalImageButton = NewObject(BUTTON_GetClass(), NULL,
	// 	GA_ID, OPEN_ORIGINAL_IMAGE_BUTTON_ID,
	// 	GA_Disabled, TRUE,
	// 	BUTTON_TextPen, sendMessageButtonPen,
	// 	#ifdef __AMIGAOS3__
	// 	BUTTON_BackgroundPen, 0,
	// 	#endif
	// 	GA_TextAttr, &uiTextAttr,
	// 	BUTTON_Justification, BCJ_CENTER,
	// 	GA_Text, (ULONG)"Open Original Image",
	// 	GA_RelVerify, TRUE,
	// 	ICA_TARGET, ICTARGET_IDCMP,
	// 	TAG_DONE)) == NULL) {
	// 		printf("Could not open original image button\n");
	// 		return RETURN_ERROR;
	// }

	// if ((saveCopyButton = NewObject(BUTTON_GetClass(), NULL,
	// 	GA_ID, SAVE_COPY_BUTTON_ID,
	// 	GA_Disabled, TRUE,
	// 	BUTTON_TextPen, sendMessageButtonPen,
	// 	#ifdef __AMIGAOS3__
	// 	BUTTON_BackgroundPen, 0,
	// 	#endif
	// 	GA_TextAttr, &uiTextAttr,
	// 	BUTTON_Justification, BCJ_CENTER,
	// 	GA_Text, (ULONG)"Save Copy",
	// 	GA_RelVerify, TRUE,
	// 	ICA_TARGET, ICTARGET_IDCMP,
	// 	TAG_DONE)) == NULL) {
	// 		printf("Could not save copy button\n");
	// 		return RETURN_ERROR;
	// }

	// if ((imageHistoryButtonsLayout = NewObject(LAYOUT_GetClass(), NULL,
	// 	LAYOUT_Orientation, LAYOUT_ORIENT_VERT,
	// 	LAYOUT_SpaceInner, TRUE,
	// 	LAYOUT_SpaceOuter, TRUE,
	// 	LAYOUT_AddChild, newImageButton,
	// 	CHILD_WeightedHeight, 5,
	// 	LAYOUT_AddChild, deleteImageButton,
	// 	CHILD_WeightedHeight, 5,
	// 	LAYOUT_AddChild, imageListBrowser,
	// 	CHILD_WeightedWidth, 90,
	// 	TAG_DONE)) == NULL) {
	// 		printf("Could not create image history buttons layout\n");
	// 		return RETURN_ERROR;
	// }

	// if ((newChatButton = NewObject(BUTTON_GetClass(), NULL,
	// 	GA_ID, NEW_CHAT_BUTTON_ID,
	// 	BUTTON_TextPen, newChatButtonPen,
	// 	#ifdef __AMIGAOS3__
	// 	BUTTON_BackgroundPen, 0,
	// 	#endif
	// 	GA_TextAttr, &uiTextAttr,
	// 	BUTTON_Justification, BCJ_CENTER,
	// 	GA_Text, (ULONG)"+ New Chat",
	// 	GA_RelVerify, TRUE,
	// 	ICA_TARGET, ICTARGET_IDCMP,
	// 	TAG_DONE)) == NULL) {
	// 		printf("Could not create new chat button\n");
	// 		return RETURN_ERROR;
	// }

	// if ((deleteChatButton = NewObject(BUTTON_GetClass(), NULL,
	// 	GA_ID, DELETE_CHAT_BUTTON_ID,
	// 	BUTTON_TextPen, deleteButtonPen,
	// 	#ifdef __AMIGAOS3__
	// 	BUTTON_BackgroundPen, 0,
	// 	#endif
	// 	GA_TextAttr, &uiTextAttr,
	// 	BUTTON_Justification, BCJ_CENTER,
	// 	GA_Text, (ULONG)"- Delete Chat",
	// 	GA_RelVerify, TRUE,
	// 	ICA_TARGET, ICTARGET_IDCMP,
	// 	TAG_DONE)) == NULL) {
	// 		printf("Could not create delete chat button\n");
	// 		return RETURN_ERROR;
	// }

	// if ((conversationListBrowser = NewObject(LISTBROWSER_GetClass(), NULL,
	// 	GA_ID, CONVERSATION_LIST_BROWSER_ID,
	// 	GA_RelVerify, TRUE,
	// 	GA_TextAttr, &uiTextAttr,
	// 	LISTBROWSER_WrapText, TRUE,
	// 	LISTBROWSER_AutoFit, TRUE,
	// 	LISTBROWSER_ShowSelected, TRUE,
	// 	LISTBROWSER_Labels, conversationList,
	// 	TAG_DONE)) == NULL) {
	// 		printf("Could not create conversation list browser\n");
	// 		return RETURN_ERROR;
	// }

	// if ((conversationsLayout = NewObject(LAYOUT_GetClass(), NULL,
	// 	LAYOUT_Orientation, LAYOUT_ORIENT_VERT,
	// 	LAYOUT_SpaceInner, TRUE,
	// 	LAYOUT_SpaceOuter, TRUE,
	// 	LAYOUT_AddChild, newChatButton,
	// 	CHILD_WeightedHeight, 5,
	// 	LAYOUT_AddChild, deleteChatButton,
	// 	CHILD_WeightedHeight, 5,
	// 	LAYOUT_AddChild, conversationListBrowser,
	// 	CHILD_WeightedWidth, 90,
	// 	TAG_DONE)) == NULL) {
	// 		printf("Could not create conversations layout\n");
	// 		return RETURN_ERROR;
	// }

	// if ((statusBar = NewObject(STRING_GetClass(), NULL,
	// 	GA_ID, STATUS_BAR_ID,
	// 	GA_RelVerify, TRUE,
	// 	GA_ReadOnly, TRUE,
	// 	GA_TextAttr, &uiTextAttr,
	// 	TAG_DONE)) == NULL) {
	// 		printf("Could not create text editor\n");
	// 		return RETURN_ERROR;
	// }

	// if ((textInputTextEditor = NewObject(TEXTEDITOR_GetClass(), NULL,
	// 	GA_ID, TEXT_INPUT_TEXT_EDITOR_ID,
	// 	GA_RelVerify, TRUE,
	// 	GA_TextAttr, &chatTextAttr,
	// 	ICA_TARGET, ICTARGET_IDCMP,
	// 	TAG_DONE)) == NULL) {
	// 		printf("Could not create text editor\n");
	// 		return RETURN_ERROR;
	// }

	// if ((chatOutputTextEditor = NewObject(TEXTEDITOR_GetClass(), NULL,
	// 	GA_ID, CHAT_OUTPUT_TEXT_EDITOR_ID,
	// 	GA_RelVerify, TRUE,
	// 	GA_ReadOnly, TRUE,
	// 	GA_TextAttr, &chatTextAttr,
	// 	ICA_TARGET, ICTARGET_IDCMP,
	// 	GA_TEXTEDITOR_ImportHook, GV_TEXTEDITOR_ImportHook_MIME,
	// 	GA_TEXTEDITOR_ExportHook, GV_TEXTEDITOR_ExportHook_Plain,
	// 	TAG_DONE)) == NULL) {
	// 		printf("Could not create text editor\n");
	// 		return RETURN_ERROR;
	// }

	// if ((chatOutputScroller = NewObject(SCROLLER_GetClass(), NULL,
	// 	GA_ID, CHAT_OUTPUT_SCROLLER_ID,
	// 	GA_RelVerify, TRUE,
	// 	ICA_TARGET, ICTARGET_IDCMP,
	// 	TAG_DONE)) == NULL) {
	// 		printf("Could not create scroller\n");
	// 		return RETURN_ERROR;
	// }

	// if ((chatInputLayout = NewObject(LAYOUT_GetClass(), NULL,
	// 	LAYOUT_Orientation, LAYOUT_ORIENT_HORIZ,
	// 	LAYOUT_HorizAlignment, LALIGN_CENTER,
	// 	LAYOUT_SpaceInner, TRUE,
	// 	LAYOUT_SpaceOuter, TRUE,
	// 	LAYOUT_AddChild, textInputTextEditor,
	// 	CHILD_WeightedWidth, 80,
	// 	CHILD_NoDispose, TRUE,
	// 	LAYOUT_AddChild, sendMessageButton,
	// 	CHILD_WeightedWidth, 20,
	// 	TAG_DONE)) == NULL) {
	// 		printf("Could not create chat input layout\n");
	// 		return RETURN_ERROR;
	// }

	// if ((chatOutputLayout = NewObject(LAYOUT_GetClass(), NULL,
	// 	LAYOUT_Orientation, LAYOUT_ORIENT_HORIZ,
	// 	LAYOUT_HorizAlignment, LALIGN_CENTER,
	// 	LAYOUT_SpaceInner, TRUE,
	// 	LAYOUT_SpaceOuter, TRUE,
	// 	LAYOUT_AddChild, chatOutputTextEditor,
	// 	CHILD_WeightedWidth, 100,
	// 	LAYOUT_AddChild, chatOutputScroller,
	// 	CHILD_WeightedWidth, 10,
	// 	TAG_DONE)) == NULL) {
	// 		printf("Could not create chat output layout\n");
	// 		return RETURN_ERROR;
	// }

	// if ((chatTextBoxesLayout = NewObject(LAYOUT_GetClass(), NULL,
	// 	LAYOUT_Orientation, LAYOUT_ORIENT_VERT,
	// 	LAYOUT_SpaceInner, TRUE,
	// 	LAYOUT_SpaceOuter, TRUE,
	// 	LAYOUT_AddChild, chatOutputLayout,
	// 	CHILD_WeightedHeight, 70,
	// 	LAYOUT_AddChild, chatInputLayout,
	// 	CHILD_WeightedHeight, 20,
	// 	LAYOUT_AddChild, statusBar,
	// 	CHILD_WeightedHeight, 10,
	// 	CHILD_NoDispose, TRUE,
	// 	TAG_DONE)) == NULL) {
	// 		printf("Could not create chat layout\n");
	// 		return RETURN_ERROR;
	// }

	// if ((chatModeLayout = NewObject(LAYOUT_GetClass(), NULL,
	// 	LAYOUT_Orientation, LAYOUT_ORIENT_HORIZ,
	// 	LAYOUT_SpaceInner, TRUE,
	// 	LAYOUT_SpaceOuter, TRUE,
	// 	LAYOUT_AddChild, conversationsLayout,
	// 	CHILD_WeightedWidth, 30,
	// 	LAYOUT_AddChild, chatTextBoxesLayout,
	// 	CHILD_WeightedWidth, 70,
	// 	TAG_DONE)) == NULL) {
	// 		printf("Could not create main layout\n");
	// 		return RETURN_ERROR;
	// }

	// if ((imageGenerationInputLayout = NewObject(LAYOUT_GetClass(), NULL,
	// 	LAYOUT_Orientation, LAYOUT_ORIENT_VERT,
	// 	LAYOUT_SpaceInner, TRUE,
	// 	LAYOUT_SpaceOuter, TRUE,
	// 	LAYOUT_AddChild, openSmallImageButton,
	// 	CHILD_WeightedHeight, 10,
	// 	LAYOUT_AddChild, openMediumImageButton,
	// 	CHILD_WeightedHeight, 10,
	// 	LAYOUT_AddChild, openLargeImageButton,
	// 	CHILD_WeightedHeight, 10,
	// 	LAYOUT_AddChild, openOriginalImageButton,
	// 	CHILD_WeightedHeight, 10,
	// 	LAYOUT_AddChild, saveCopyButton,
	// 	CHILD_WeightedHeight, 10,
	// 	LAYOUT_AddChild, textInputTextEditor,
	// 	CHILD_WeightedHeight, 40,
	// 	CHILD_NoDispose, TRUE,
	// 	LAYOUT_AddChild, createImageButton,
	// 	CHILD_WeightedHeight, 10,
	// 	TAG_DONE)) == NULL) {
	// 		printf("Could not create image generation input layout\n");
	// 		return RETURN_ERROR;
	// }

	// if ((imageGenerationTextBoxesLayout = NewObject(LAYOUT_GetClass(), NULL,
	// 	LAYOUT_Orientation, LAYOUT_ORIENT_VERT,
	// 	LAYOUT_SpaceInner, TRUE,
	// 	LAYOUT_SpaceOuter, TRUE,
	// 	LAYOUT_AddChild, imageGenerationInputLayout,
	// 	CHILD_WeightedHeight, 90,
	// 	LAYOUT_AddChild, statusBar,
	// 	CHILD_WeightedHeight, 10,
	// 	CHILD_NoDispose, TRUE,
	// 	TAG_DONE)) == NULL) {
	// 		printf("Could not create chat layout\n");
	// 		return RETURN_ERROR;
	// }

	// if ((imageGenerationModeLayout = NewObject(LAYOUT_GetClass(), NULL,
	// 	LAYOUT_Orientation, LAYOUT_ORIENT_HORIZ,
	// 	LAYOUT_SpaceInner, TRUE,
	// 	LAYOUT_SpaceOuter, TRUE,
	// 	LAYOUT_AddChild, imageHistoryButtonsLayout,
	// 	CHILD_WeightedWidth, 30,
	// 	LAYOUT_AddChild, imageGenerationTextBoxesLayout,
	// 	CHILD_WeightedWidth, 70,
	// 	TAG_DONE)) == NULL) {
	// 		printf("Could not create image generation layout\n");
	// 		return RETURN_ERROR;
	// }

	// if ((modeClickTabOld = NewObject(CLICKTAB_GetClass(), NULL,
	// 	GA_ID, CLICKTAB_MODE_SELECTION_ID,
	// 	GA_RelVerify, TRUE,
	// 	CLICKTAB_Labels, modeSelectionTabList,
	// 	CLICKTAB_AutoFit, TRUE,
	// 	CLICKTAB_PageGroup, NewObject(PAGE_GetClass(), NULL,
    //         PAGE_Add,       chatModeLayout,
    //         PAGE_Add,       imageGenerationModeLayout,
    //     TAG_DONE),
	// 	TAG_DONE)) == NULL) {
	// 		printf("Could not create mode click tab\n");
	// 		return RETURN_ERROR;
	// }

	// Object *layoutToOpen;
	// if (isAmigaOS3X) {
	// 	switch (selectedMode) {
	// 		case MODE_SELECTION_TAB_CHAT_ID:
	// 			layoutToOpen = chatModeLayout;
	// 			break;
	// 		case MODE_SELECTION_TAB_IMAGE_GENERATION_ID:
	// 			layoutToOpen = imageGenerationModeLayout;
	// 			break;
	// 		default:
	// 			layoutToOpen = chatModeLayout;
	// 			break;
	// 	}
	// } else {
	// 	layoutToOpen = modeClickTabOld;
	// }

	appPort = CreateMsgPort();

	// if (!isPublicScreen) {
	// 	SetGadgetAttrs(textInputTextEditor, mainWindow, NULL, GA_TEXTEDITOR_ColorMap, &textEditorColorMap, TAG_DONE);
	// 	if (!isAmigaOS3X || selectedMode == MODE_SELECTION_TAB_CHAT_ID)
	// 		SetGadgetAttrs(chatOutputTextEditor, mainWindow, NULL, GA_TEXTEDITOR_ColorMap, &textEditorColorMap, TAG_DONE);
	// }

	refreshOpenAIMenuItems();
	refreshSpeechMenuItems();

	// For some reason it won't let you paste text into the empty text editor unless you do this
	// DoGadgetMethod(textInputTextEditor, mainWindow, NULL, GM_TEXTEDITOR_InsertText, NULL, "", GV_TEXTEDITOR_InsertText_Bottom);

	updateStatusBar("Ready", 5);
	
	// if (!isAmigaOS3X || selectedMode == MODE_SELECTION_TAB_CHAT_ID) {
	// 	ActivateLayoutGadget(chatModeLayout, mainWindow, NULL, textInputTextEditor);
	// } else {
	// 	ActivateLayoutGadget(imageGenerationModeLayout, mainWindow, NULL, textInputTextEditor);
	// }

	return RETURN_OK;
}

/**
 * Update the status bar
 * @param message the message to display
 * @param pen the pen to use for the text
 * 
**/ 
void updateStatusBar(CONST_STRPTR message, const ULONG pen) {
	// SetGadgetAttrs(statusBar, mainWindow, NULL, STRINGA_Pens, isPublicScreen ? ObtainBestPen(screen->ViewPort.ColorMap, config.colors[3*pen+1], config.colors[3*pen+2], config.colors[3*pen+3], OBP_Precision, PRECISION_GUI, TAG_DONE) : pen, TAG_DONE);
	// SetGadgetAttrs(statusBar, mainWindow, NULL, STRINGA_TextVal, message, TAG_DONE);
}

/**
 * Display a requester for the screen the application should open on and the mode to run in (if AmigaOS 3.X)
 * @return RETURN_OK on success, RETURN_ERROR on failure
**/
static LONG openStartupOptions() {
	Object *screenSelectRadioButton, *startupOptionsOkButton, *screenSelectGroup, *startupOptionsWindowObject = NULL;
	struct ScreenModeRequester *screenModeRequester;
	screen = LockPubScreen("Workbench");

	if (config.uiFontName != NULL) {
		screenFont.ta_Name = config.uiFontName;
		screenFont.ta_YSize = config.uiFontSize;
		screenFont.ta_Style = config.uiFontStyle;
		screenFont.ta_Flags = config.uiFontFlags;
	}

	CONST_STRPTR radioButtonOptions[] = {
		"Open in Workbench",
		"New screen",
		NULL
	};

	if (!(screenSelectRadioButton = RadioObject,
		MUIA_Frame, MUIV_Frame_Group,
		MUIA_FrameTitle, "Screen to open:",
		MUIA_HelpNode, "radioButton",
		MUIA_Radio_Entries, radioButtonOptions,
	End)) {
		printf("Could not create screenSelectRadioButton\n");
		return RETURN_ERROR;
	}

	if (startupOptionsOkButton = MUI_MakeObject(MUIO_Button, "OK")) {
		set(startupOptionsOkButton, MUIA_CycleChain, 1);
	} else {
		printf("Could not create startupOptionsOkButton\n");
		return RETURN_ERROR;
	}

	if (!(screenSelectGroup = GroupObject,	
		Child, screenSelectRadioButton,
		Child, MUI_MakeObject(MUIO_HBar,10),
		Child, startupOptionsOkButton = startupOptionsOkButton,
	End)) {
		printf("Could not create screenSelectGroup\n");
		return RETURN_ERROR;
	}

	if (!(startupOptionsWindowObject = WindowObject,
		MUIA_Window_Title, "Startup Options",
		MUIA_Window_ID, SCREEN_SELECT_WINDOW_ID,
		MUIA_Window_CloseGadget, TRUE,
		MUIA_Window_DepthGadget, FALSE,
		MUIA_Window_SizeGadget, FALSE,
		WindowContents, screenSelectGroup,
	End)) {
		printf("Could not create startupOptionsWindowObject\n");
		return RETURN_ERROR;
	}

	DoMethod(startupOptionsWindowObject, MUIM_Notify, MUIA_Window_CloseRequest, TRUE,
	  app, 2, MUIM_Application_ReturnID, MUIV_Application_ReturnID_Quit);

	DoMethod(app, OM_ADDMEMBER, startupOptionsWindowObject);

	set(startupOptionsWindowObject, MUIA_Window_Open, TRUE);
		  
	DoMethod(startupOptionsOkButton, MUIM_Notify, MUIA_Pressed, FALSE,
          app, 2, MUIM_Application_ReturnID, STARTUP_OPTIONS_OK_BUTTON_PRESS);

	BOOL done = FALSE;
	ULONG signals;

	while (!done) {
		ULONG id = DoMethod(app, MUIM_Application_NewInput, &signals);

		switch(id) {
			case MUIV_Application_ReturnID_Quit:
				done = TRUE;
				break;
			case STARTUP_OPTIONS_OK_BUTTON_PRESS:
				{
					ULONG selectedRadioButton;
					get(screenSelectRadioButton, MUIA_Radio_Active, &selectedRadioButton);

					if (selectedRadioButton == 0) {
						// Open in Workbench
						isPublicScreen = TRUE;
						done = TRUE;
					} else {
						// New screen
						ULONG displayID = GetVPModeID(&screen->ViewPort);
						if (screenModeRequester = (struct ScreenModeRequester *)MUI_AllocAslRequestTags(ASL_ScreenModeRequest,
						ASLSM_DoWidth, TRUE,
						ASLSM_DoHeight, TRUE,
						ASLSM_DoDepth, TRUE,
						ASLSM_DoOverscanType, TRUE,
						ASLSM_DoAutoScroll, TRUE,
						ASLSM_InitialDisplayID, displayID,
						ASLSM_InitialDisplayWidth, screen->Width,
						ASLSM_InitialDisplayHeight, screen->Height,
						ASLSM_InitialOverscanType, OSCAN_TEXT,
						ASLSM_InitialDisplayDepth, 4,
						ASLSM_MinDepth, 4,
						ASLSM_NegativeText, NULL,
						TAG_DONE)) {
							if (MUI_AslRequestTags(screenModeRequester, TAG_DONE)) {
								isPublicScreen = FALSE;
								UnlockPubScreen(NULL, screen);
								for (WORD i = 0; i < NUMDRIPENS; i++) {
									pens[i]= 1;
								}
								pens[DETAILPEN] = 4; // nothing?
								pens[BLOCKPEN] = 4; // nothing?
								pens[TEXTPEN] = 1; // text colour
								pens[SHINEPEN] = 1; // gadget top and left borders
								pens[SHADOWPEN] = 1; // gadget bottom and right borders
								pens[FILLPEN] = 2; // button text
								pens[FILLTEXTPEN] = 4; // title bar text
								pens[BACKGROUNDPEN] = 3; // background
								pens[HIGHLIGHTTEXTPEN] = 4; // nothing?
								pens[BARDETAILPEN] = 1; // menu text
								pens[BARBLOCKPEN] = 0; // menu background
								pens[BARTRIMPEN] = 1; // nothing?
								#ifdef __AMIGAOS4__
								pens[FOREGROUNDPEN] = 0;
								pens[DISABLEDPEN] = 8;
								pens[DISABLEDSHADOWPEN] = 7;
								pens[DISABLEDSHINEPEN] = 6;
								pens[DISABLEDTEXTPEN] = 3;
								pens[MENUBACKGROUNDPEN] = 9;
								pens[MENUTEXTPEN] = 3;
								pens[MENUSHINEPEN] = 8;
								pens[MENUSHADOWPEN] = 0;
								pens[SELECTPEN] = 2;
								pens[SELECTTEXTPEN] = 4;
								#endif
								pens[NUMDRIPENS] = ~0;

								if ((screen = OpenScreenTags(NULL,
									SA_Pens, (ULONG)pens,
									SA_LikeWorkbench, TRUE,
									SA_DisplayID, screenModeRequester->sm_DisplayID,
									SA_Depth, screenModeRequester->sm_DisplayDepth,
									SA_Overscan, screenModeRequester->sm_OverscanType,
									SA_AutoScroll, screenModeRequester->sm_AutoScroll,
									SA_Width, screenModeRequester->sm_DisplayWidth,
									SA_Height, screenModeRequester->sm_DisplayHeight,
									SA_Font, &screenFont,
									SA_Colors32, config.colors,
									TAG_DONE)) == NULL) {
										printf("Could not open screen\n");
										MUI_FreeAslRequest(screenModeRequester);
										return RETURN_ERROR;
								}
								MUI_FreeAslRequest(screenModeRequester);
								done = TRUE;
							}
						}
					}
					break;
				}
			}
		if (!done && signals) {
			signals = Wait(signals | SIGBREAKF_CTRL_C);
		}
	}

	set(startupOptionsWindowObject,MUIA_Window_Open,FALSE);

	return RETURN_OK;
}

/**
 * Format a string with escape sequences into a string with the actual characters
 * @param unformattedText the text to format
**/
static void formatText(STRPTR unformattedText) {
	LONG newStringIndex = 0;
	const LONG oldStringLength = strlen(unformattedText);
	if (oldStringLength == 0) return;
	for (LONG oldStringIndex = 0; oldStringIndex < oldStringLength; oldStringIndex++) {
		if (unformattedText[oldStringIndex] == '\\') {
			if (unformattedText[oldStringIndex + 1] == 'n') {
				unformattedText[newStringIndex++] = '\n';
			} else if (unformattedText[oldStringIndex + 1] == 'r') {
				unformattedText[newStringIndex++] = '\r';
			} else if (unformattedText[oldStringIndex + 1] == 't') {
				unformattedText[newStringIndex++] = '\t';
			}
			oldStringIndex++;
		} else {
			unformattedText[newStringIndex++] = unformattedText[oldStringIndex];
		}
	}
	unformattedText[newStringIndex++] = '\0';
}

/**
 * Get the message content from the JSON response from OpenAI
 * @param json the JSON response from OpenAI
 * @param stream whether the response is a stream or not
 * @return a pointer to a new string containing the message content -- Free it with FreeVec() when you are done using it
 * If found role in the json instead of content then return an empty string
 * @todo Handle errors
**/
static STRPTR getMessageContentFromJson(struct json_object *json, BOOL stream) {
	if (json == NULL) return NULL;
	STRPTR json_str = json_object_to_json_string(json);
	struct json_object *choices = json_object_object_get(json, "choices");
	struct json_object *choice = json_object_array_get_idx(choices, 0);
	struct json_object *message = json_object_object_get(choice, stream ? "delta" : "message");
	if (stream) {
		struct json_object *role = json_object_object_get(message, "role");
		if (role != NULL)
			return "\0";
	}
	struct json_object *content = json_object_object_get(message, "content");
	return json_object_get_string(content);
}

/**
 * Sends a chat message to the OpenAI API and displays the response and speaks it if speech is enabled
**/
static void sendChatMessage() {
	BOOL isNewConversation = FALSE;
	STRPTR finishReason = NULL;
	struct json_object **responses;
	if (currentConversation == NULL) {
		isNewConversation = TRUE;
		currentConversation = newConversation();
	}
	set(sendMessageButton, MUIA_Disabled, TRUE);
	// SetGadgetAttrs(newChatButton, mainWindow, NULL, GA_Disabled, TRUE, TAG_DONE);
	// SetGadgetAttrs(deleteChatButton, mainWindow, NULL, GA_Disabled, TRUE, TAG_DONE);

	updateStatusBar("Sending message...", 7);
	STRPTR receivedMessage = AllocVec(READ_BUFFER_LENGTH, MEMF_ANY | MEMF_CLEAR);
	STRPTR text;
	DoMethod(chatInputTextEditor, MUIM_TextEditor_ExportText, &text);

	// STRPTR text = DoGadgetMethod(textInputTextEditor, mainWindow, NULL, GM_TEXTEDITOR_ExportText, NULL);
	// Remove trailing newline characters
	while (text[strlen(text) - 1] == '\n') {
		text[strlen(text) - 1] = '\0';
	}
	STRPTR textUTF_8 = ISO8859_1ToUTF8(text);
	addTextToConversation(currentConversation, textUTF_8, "user");
	displayConversation(currentConversation);
	set(sendMessageButton, MUIA_Disabled, TRUE);
	DoMethod(chatInputTextEditor, MUIM_TextEditor_ClearText);
	DoMethod(chatInputTextEditor, MUIM_GoActive);

	BOOL dataStreamFinished = FALSE;
	ULONG speechIndex = 0;
	UWORD wordNumber = 0;

	DoMethod(chatOutputTextEditor, MUIM_TextEditor_InsertText, "\n", MUIV_TextEditor_InsertText_Bottom);
	do {
		if (config.chatSystem != NULL && (config.chatSystem) > 0)
			addTextToConversation(currentConversation, config.chatSystem, "system");
		responses = postChatMessageToOpenAI(currentConversation, config.chatModel, config.openAiApiKey, TRUE);
		if (responses == NULL) {
			displayError("Could not connect to OpenAI");
			set(sendMessageButton, MUIA_Disabled, FALSE);
			// SetGadgetAttrs(newChatButton, mainWindow, NULL, GA_Disabled, FALSE, TAG_DONE);
			// SetGadgetAttrs(deleteChatButton, mainWindow, NULL, GA_Disabled, FALSE, TAG_DONE);
			FreeVec(receivedMessage);
			return;
		}
		if (config.chatSystem != NULL && strlen(config.chatSystem) > 0) {
			struct MinNode *chatSystemNode = RemTail(currentConversation);
			FreeVec(chatSystemNode);
		}
		UWORD responseIndex = 0;
		struct json_object *response;
		while (response = responses[responseIndex++]) {
			struct json_object *error;
			if (json_object_object_get_ex(response, "error", &error)) {
				struct json_object *message = json_object_object_get(error, "message");
				STRPTR messageString = json_object_get_string(message);
				displayError(messageString);
				set(chatInputTextEditor, MUIA_TextEditor_Contents, text);
				struct MinNode *lastMessage = RemTail(currentConversation);
				FreeVec(lastMessage);
				if (currentConversation == currentConversation->messages->mlh_TailPred) {
					freeConversation(currentConversation);
					currentConversation = NULL;
					DoMethod(chatOutputTextEditor, MUIM_TextEditor_ClearText);
				} else {
					displayConversation(currentConversation);
				}
				json_object_put(response);

				set(sendMessageButton, MUIA_Disabled, FALSE);

				// SetGadgetAttrs(newChatButton, mainWindow, NULL, GA_Disabled, FALSE, TAG_DONE);
				// SetGadgetAttrs(deleteChatButton, mainWindow, NULL, GA_Disabled, FALSE, TAG_DONE);

				FreeVec(receivedMessage);
				return;
			}
			STRPTR contentString = getMessageContentFromJson(response, TRUE);
			if (contentString != NULL) {
				// Text for printing
				formatText(contentString);
				STRPTR formattedMessageISO8859_1 = UTF8ToISO8859_1(contentString);
				strncat(receivedMessage, contentString, READ_BUFFER_LENGTH - strlen(receivedMessage) - 1);
				DoMethod(chatOutputTextEditor, MUIM_TextEditor_InsertText, formattedMessageISO8859_1, MUIV_TextEditor_InsertText_Bottom);
				FreeVec(formattedMessageISO8859_1);
				FreeVec(contentString);
				// Text for speaking
				STRPTR unformattedMessageISO8859_1 = UTF8ToISO8859_1(receivedMessage);
				if (++wordNumber % 50 == 0) {
					if (config.speechEnabled) {
						if (config.speechSystem != SPEECH_SYSTEM_OPENAI) {
							speakText(unformattedMessageISO8859_1 + speechIndex);
						}
						speechIndex = strlen(unformattedMessageISO8859_1);
						FreeVec(unformattedMessageISO8859_1);
					}
				}
				finishReason = json_object_get_string(json_object_object_get(response, "finish_reason"));
				if (finishReason != NULL) {
					dataStreamFinished = TRUE;
				}
				json_object_put(response);
			} else {
				dataStreamFinished = TRUE;
			}
		}
	} while (!dataStreamFinished);

	if (responses != NULL) {
		addTextToConversation(currentConversation, receivedMessage, "assistant");
		if (config.speechEnabled) {
			STRPTR receivedMessageISO8859_1 = UTF8ToISO8859_1(receivedMessage);
			if (config.speechSystem == SPEECH_SYSTEM_OPENAI) {
				speakText(receivedMessage);
			} else {
				speakText(receivedMessageISO8859_1 + speechIndex);
			}
			FreeVec(receivedMessageISO8859_1);
		}
		FreeVec(responses);
		FreeVec(receivedMessage);
		updateStatusBar("Ready", 5);
		if (isNewConversation) {
			updateStatusBar("Generating conversation title...", 7);
			addTextToConversation(currentConversation, "generate a short title for this conversation and don't enclose the title in quotes or prefix the response with anything", "user");
			responses = postChatMessageToOpenAI(currentConversation, config.chatModel, config.openAiApiKey, FALSE);
			if (responses == NULL) {
				displayError("Could not connect to OpenAI");
				// SetGadgetAttrs(sendMessageButton, mainWindow, NULL, GA_Disabled, FALSE, TAG_DONE);
				// SetGadgetAttrs(newChatButton, mainWindow, NULL, GA_Disabled, FALSE, TAG_DONE);
				// SetGadgetAttrs(deleteChatButton, mainWindow, NULL, GA_Disabled, FALSE, TAG_DONE);
				return;
			}
			if (responses[0] != NULL) {
				STRPTR responseString = getMessageContentFromJson(responses[0], FALSE);
				formatText(responseString);
				if (currentConversation->name == NULL) {
					currentConversation->name = AllocVec(strlen(responseString) + 1, MEMF_CLEAR);
					strncpy(currentConversation->name, responseString, strlen(responseString));
				}
				DoMethod(conversationListObject, MUIM_NList_InsertSingle, currentConversation, MUIV_NList_Insert_Top);
			}
			struct MinNode *titleRequestNode = RemTail(currentConversation);
			FreeVec(titleRequestNode);
			json_object_put(responses[0]);
			FreeVec(responses);
		}
	}

	updateStatusBar("Ready", 5);
	
	// SetGadgetAttrs(sendMessageButton, mainWindow, NULL, GA_Disabled, FALSE, TAG_DONE);
	// SetGadgetAttrs(newChatButton, mainWindow, NULL, GA_Disabled, FALSE, TAG_DONE);
	// SetGadgetAttrs(deleteChatButton, mainWindow, NULL, GA_Disabled, FALSE, TAG_DONE);

	// FreeVec(text);
	// FreeVec(textUTF_8);
}

/**
 * Updates the menu
 */
static void updateMenu() {
	set(mainWindowObject, MUIA_Window_Menustrip, MUI_MakeObject(MUIO_MenustripNM, amigaGPTMenu));
}

/**
 * Sets the checkboxesfor the OpenAI menu
**/
static void refreshOpenAIMenuItems() {
	struct NewMenu *newMenu = amigaGPTMenu;
	while ((int)newMenu->nm_UserData != MENU_ITEM_CHAT_MODEL) {
		newMenu++;
	}

	while ((++newMenu)->nm_Type == NM_SUB) {
		if (strcmp(newMenu->nm_Label, CHAT_MODEL_NAMES[config.chatModel]) == 0 &&
		strlen(newMenu->nm_Label) == strlen(CHAT_MODEL_NAMES[config.chatModel])) {
			newMenu->nm_Flags |= CHECKED;
		} else {
			newMenu->nm_Flags &= ~CHECKED;
		}
	}

	while ((int)newMenu->nm_UserData != MENU_ITEM_IMAGE_MODEL) {
		newMenu++;
	}

	while ((++newMenu)->nm_Type == NM_SUB) {
		if (strcmp(newMenu->nm_Label, IMAGE_MODEL_NAMES[config.imageModel]) == 0) {
			newMenu->nm_Flags |= CHECKED;
		} else {
			newMenu->nm_Flags &= ~CHECKED;
		}
	}

	while ((int)newMenu->nm_UserData != MENU_ITEM_IMAGE_SIZE_DALL_E_2) {
		newMenu++;
	}

	while ((++newMenu)->nm_Type == NM_SUB) {
		if (strcmp(newMenu->nm_Label, IMAGE_SIZE_NAMES[config.imageSizeDallE2]) == 0) {
			newMenu->nm_Flags |= CHECKED;
		} else {
			newMenu->nm_Flags &= ~CHECKED;
		}
	}

	while ((int)newMenu->nm_UserData != MENU_ITEM_IMAGE_SIZE_DALL_E_3) {
		newMenu++;
	}

	while ((++newMenu)->nm_Type == NM_SUB) {
		if (strcmp(newMenu->nm_Label, IMAGE_SIZE_NAMES[config.imageSizeDallE3]) == 0) {
			newMenu->nm_Flags |= CHECKED;
		} else {
			newMenu->nm_Flags &= ~CHECKED;
		}
	}

	updateMenu();
}

/**
 * Sets the checkboxes for the speech options that are currently selected
**/
static void refreshSpeechMenuItems() {
	struct NewMenu *newMenu = amigaGPTMenu;
	while ((int)newMenu->nm_UserData != MENU_ITEM_SPEECH_ENABLED) {
		newMenu++;
	}

	if (config.speechEnabled) {
		newMenu++->nm_Flags |= CHECKED;
	} else {
		newMenu++->nm_Flags &= ~CHECKED;
	}

	while ((++newMenu)->nm_Type == NM_SUB) {
		if (strcmp(newMenu->nm_Label, SPEECH_SYSTEM_NAMES[config.speechSystem]) == 0) {
			newMenu->nm_Flags |= CHECKED;
		} else {
			newMenu->nm_Flags &= ~CHECKED;
		}
	}

	#ifdef __AMIGAOS4__
	while ((++newMenu)->nm_Type == NM_SUB) {
		CONST_STRPTR currentFliteVoiceName = SPEECH_FLITE_VOICE_NAMES[config.speechFliteVoice];
		// Find the length of the first word in the label
		UBYTE labelLen = 0;
		while (newMenu->nm_Label[labelLen] != ' ') {
			labelLen++;
		}

		// Compare the first word of the label with currentVoiceName
		// We have to do this because the (slow) warnings were appended to the label
		if (strncmp(newMenu->nm_Label, currentFliteVoiceName, labelLen) == 0 && currentFliteVoiceName[labelLen] == '\0') {
			newMenu->nm_Flags |= CHECKED;
		} else {
			newMenu->nm_Flags &= ~CHECKED;
		}
	}
	#endif

	#ifdef __AMIGAOS3__
	// Skip the Accent menu
	newMenu++;
	#endif

	while ((++newMenu)->nm_Type == NM_SUB) {
		CONST_STRPTR currentOpenAITTSVoiceName = OPENAI_TTS_VOICE_NAMES[config.openAITTSVoice];

		if (strncmp(newMenu->nm_Label, currentOpenAITTSVoiceName, strlen(newMenu->nm_Label)) == 0 &&
		 strlen(newMenu->nm_Label) == strlen(currentOpenAITTSVoiceName)) {
			newMenu->nm_Flags |= CHECKED;
		} else {
			newMenu->nm_Flags &= ~CHECKED;
		}
	}

	while ((++newMenu)->nm_Type == NM_SUB) {
		CONST_STRPTR currentOpenAITTSModelName = OPENAI_TTS_MODEL_NAMES[config.openAITTSModel];

		if (strncmp(newMenu->nm_Label, currentOpenAITTSModelName, strlen(newMenu->nm_Label)) == 0 &&
		 strlen(newMenu->nm_Label) == strlen(currentOpenAITTSModelName)) {
			newMenu->nm_Flags |= CHECKED;
		} else {
			newMenu->nm_Flags &= ~CHECKED;
		}
	}

	updateMenu();
}

/**
 * Creates a new conversation
 * @return A pointer to the new conversation
**/
static struct Conversation* newConversation() {
	struct Conversation *conversation = AllocVec(sizeof(struct Conversation), MEMF_CLEAR);
	struct MinList *messages = AllocVec(sizeof(struct MinList), MEMF_CLEAR);
	
	// NewMinList(conversation); // This is what makes us require exec.library 45. Replace with the following:
	if (messages) {
		messages->mlh_Tail = 0;
		messages->mlh_Head = (struct MinNode *)&messages->mlh_Tail;
		messages->mlh_TailPred = (struct MinNode *)&messages->mlh_Head;
	}

	conversation->messages = messages;
	conversation->name = NULL;
	
	return conversation;
}

/**
 * Add a block of text to the conversation list
 * @param conversation The conversation to add the text to
 * @param text The text to add to the conversation
 * @param role The role of the text (user or assistant)
**/
static void addTextToConversation(struct Conversation *conversation, STRPTR text, STRPTR role) {
	struct ConversationNode *conversationNode = AllocVec(sizeof(struct ConversationNode), MEMF_CLEAR);
	if (conversationNode == NULL) {
		printf("Failed to allocate memory for conversation node\n");
		return;
	}
	strncpy(conversationNode->role, role, sizeof(conversationNode->role) - 1);
	conversationNode->role[sizeof(conversationNode->role) - 1] = '\0';
	conversationNode->content = AllocVec(strlen(text) + 1, MEMF_CLEAR);
	strncpy(conversationNode->content, text, strlen(text));
	AddTail(conversation->messages, (struct Node *)conversationNode);
}

/**
 * Add an image to the image list
 * @param image The image to add to the imagelist
**/
static void addImageToImageList(struct GeneratedImage *image) {
	// struct Node *node;
	// if ((node = AllocListBrowserNode(1,
	// 	LBNCA_CopyText, TRUE,
	// 	LBNCA_Text, image->name,
	// 	LBNA_UserData, (ULONG)image,
	// 	TAG_DONE)) == NULL) {
	// 		printf("Could not create image list browser node\n");
	// 		return RETURN_ERROR;
	// }

	// SetGadgetAttrs(imageListBrowser, mainWindow, NULL, LISTBROWSER_Labels, ~0, TAG_DONE);
	// AddHead(imageList, node);
	// SetGadgetAttrs(imageListBrowser, mainWindow, NULL, LISTBROWSER_Labels, imageList, TAG_DONE);
}

// /**
//  * Get a conversation from the conversation list
//  * @param conversationList The conversation list to get the conversation from
//  * @param index The index of the conversation to get
//  * @return A pointer to the conversation
// **/
// static struct Conversation* getConversationFromConversationList(struct List *conversationList, ULONG index) {
// 	struct Node *node = conversationList->lh_Head->ln_Succ;
// 	while (index > 0) {
// 		node = node->ln_Succ;
// 		index--;
// 	}
// 	struct MinList *conversation;
// 	GetListBrowserNodeAttrs(node, LBNA_UserData, &conversation, TAG_END);
// 	return conversation;
// }

/**
 * Prints the conversation to the conversation window
 * @param conversation the conversation to display
**/
static void displayConversation(struct Conversation *conversation) {
	struct ConversationNode *conversationNode;
	// DoGadgetMethod(chatOutputTextEditor, mainWindow, NULL, GM_TEXTEDITOR_ClearText, NULL);
	STRPTR conversationString = AllocVec(WRITE_BUFFER_LENGTH, MEMF_CLEAR);

	for (conversationNode = (struct ConversationNode *)conversation->messages->mlh_Head;
		 conversationNode->node.mln_Succ != NULL;
		 conversationNode = (struct ConversationNode *)conversationNode->node.mln_Succ) {
			if ((strlen(conversationString) + strlen(conversationNode->content) + 5) > WRITE_BUFFER_LENGTH) {
				displayError("The conversation has exceeded the maximum length.\n\nPlease start a new conversation.");
				// SetGadgetAttrs(sendMessageButton, mainWindow, NULL, GA_Disabled, TRUE, TAG_DONE);
				return;
			}
			if (strcmp(conversationNode->role, "user") == 0) {
				STRPTR content = conversationNode->content;
				strncat(conversationString, "*", WRITE_BUFFER_LENGTH - strlen(conversationString) - 2);
				while (*content != '\0') {
					if (*content == '\n') {
						strncat(conversationString, "*", WRITE_BUFFER_LENGTH - strlen(conversationString) - 2);
						strncat(conversationString, content++, 1);
						strncat(conversationString, "*", WRITE_BUFFER_LENGTH - strlen(conversationString) - 2);
					} else {
						strncat(conversationString, content++, 1);
					}
				}
				strncat(conversationString, "*\n\n", WRITE_BUFFER_LENGTH - strlen(conversationString) - 4);
			} else if (strcmp(conversationNode->role, "assistant") == 0) {
				strncat(conversationString, conversationNode->content, WRITE_BUFFER_LENGTH - strlen(conversationString));
				strncat(conversationString, "\n\n", WRITE_BUFFER_LENGTH - strlen(conversationString) - 3);	
			}	
	}

	STRPTR conversationStringISO8859_1 = UTF8ToISO8859_1(conversationString);
	// SetGadgetAttrs(chatOutputTextEditor, mainWindow, NULL, GA_TEXTEDITOR_Contents, conversationStringISO8859_1, TAG_DONE);
	// Delay(2);
	// SetGadgetAttrs(chatOutputTextEditor, mainWindow, NULL, GA_TEXTEDITOR_CursorY, ~0, TAG_DONE);
	// SetGadgetAttrs(sendMessageButton, mainWindow, NULL, GA_Disabled, FALSE, TAG_DONE);
	set(chatOutputTextEditor, MUIA_TextEditor_Contents, conversationStringISO8859_1);
	FreeVec(conversationString);
	FreeVec(conversationStringISO8859_1);
}

/**
 * Free the conversation
 * @param conversation The conversation to free
**/
static void freeConversation(struct Conversation *conversation) {
	struct ConversationNode *conversationNode;
	while ((conversationNode = (struct ConversationNode *)RemHead(conversation->messages)) != NULL) {
		FreeVec(conversationNode->content);
		FreeVec(conversationNode);
	}
	if (conversation->name != NULL)
	 	FreeVec(conversation->name);
	FreeVec(conversation);
}

static struct Conversation* copyConversation(struct Conversation *conversation) {
	struct Conversation *copy = newConversation();
	struct ConversationNode *conversationNode;
	for (conversationNode = (struct ConversationNode *)conversation->messages->mlh_Head;
		 conversationNode->node.mln_Succ != NULL;
		 conversationNode = (struct ConversationNode *)conversationNode->node.mln_Succ) {
			addTextToConversation(copy, conversationNode->content, conversationNode->role);
	}
	if (conversation->name != NULL) {
		copy->name = AllocVec(strlen(conversation->name) + 1, MEMF_CLEAR);
		strncpy(copy->name, conversation->name, strlen(conversation->name));
	}
	return copy;
}

/**
 * Free the conversation list
**/
static void freeConversationList() {
	// struct Node *conversationListNode;
	// while ((conversationListNode = RemHead(conversationList)) != NULL) {
	// 	ULONG *conversation;
	// 	GetListBrowserNodeAttrs(conversationListNode, LBNA_UserData, (ULONG *)&conversation, TAG_END);
	// 	freeConversation(conversation);
	// 	FreeVec(conversationListNode);
	// }
	// FreeVec(conversationList);
}

/**
 * Free the image list
**/
static void freeImageList() {
	// struct Node *imageListNode;
	// while ((imageListNode = RemHead(imageList)) != NULL) {
	// 	struct GeneratedImage *generatedImage;
	// 	GetListBrowserNodeAttrs(imageListNode, LBNA_UserData, (ULONG *)&generatedImage, TAG_END);
	// 	FreeVec(generatedImage->filePath);
	// 	FreeVec(generatedImage->name);
	// 	FreeVec(generatedImage->prompt);
	// 	FreeVec(generatedImage);
	// }
	// FreeVec(imageList);
}

/**
 * Free the mode selection tab list
**/
static void freeModeSelectionTabList() {
	// struct Node *modeSelectionTabListNode;
	// while ((modeSelectionTabListNode = RemHead(modeSelectionTabList)) != NULL) {
	// 	FreeClickTabNode(modeSelectionTabListNode);
	// }
}

/**
 * Remove a conversation from the conversation list
 * @param conversation The conversation to remove from the conversation list
**/
static void removeConversationFromConversationList(struct Conversation *conversation) {
	// if (conversation == NULL) return;
	// struct Node *node = conversationList->lh_Head;
	// while (node != NULL) {
	// 	struct MinList *listBrowserConversation;
	// 	GetListBrowserNodeAttrs(node, LBNA_UserData, (struct MinList *)&listBrowserConversation, TAG_END);
	// 	if (listBrowserConversation == conversation) {
	// 		// SetGadgetAttrs(conversationListBrowser, mainWindow, NULL, LISTBROWSER_Selected, -1, TAG_DONE);
	// 		// SetGadgetAttrs(conversationListBrowser, mainWindow, NULL, LISTBROWSER_Labels, ~0, TAG_DONE);
	// 		Remove(node);
	// 		FreeListBrowserNode(node);
	// 		// SetGadgetAttrs(conversationListBrowser, mainWindow, NULL, LISTBROWSER_Labels, conversationList, TAG_DONE);
	// 		freeConversation(conversation);
	// 		return;
	// 	}
	// 	node = node->ln_Succ;
	// }
}

/**
 * Save a copy of the image
 * @param image The image to save a copy of
**/ 
static void saveImageCopy(struct GeneratedImage *image) {
	if (image == NULL) return;
	STRPTR filePath = image->filePath;
	struct FileRequester *fileReq = AllocAslRequestTags(ASL_FileRequest, TAG_END);
	if (fileReq != NULL) {
		if (AslRequestTags(fileReq,
		ASLFR_Window, mainWindow,
		ASLFR_TitleText, "Save Image Copy",
		ASLFR_InitialFile, "image.png",
		ASLFR_InitialDrawer, "SYS:",
		ASLFR_DoSaveMode, TRUE,
		TAG_DONE)) {
			STRPTR savePath = fileReq->fr_Drawer;
			STRPTR saveName = fileReq->fr_File;
			BOOL isRootDirectory = savePath[strlen(savePath) - 1] == ':';
			STRPTR fullPath = AllocVec(strlen(savePath) + strlen(saveName) + 2, MEMF_CLEAR);
			snprintf(fullPath, strlen(savePath) + strlen(saveName) + 2, "%s%s%s", savePath, isRootDirectory ? "" : "/", saveName);
			copyFile(filePath, fullPath);
			FreeVec(fullPath);
		}
		FreeAslRequest(fileReq);
	}
}

/**
 * Remove an image from the image list
 * @param image The image to remove from the image list
**/
static void removeImageFromImageList(struct GeneratedImage *image) {
	// if (image == NULL) return;
	// struct Node *node = imageList->lh_Head;
	// while (node != NULL) {
	// 	struct GeneratedImage *listBrowserImage;
	// 	GetListBrowserNodeAttrs(node, LBNA_UserData, (struct GeneratedImage *)&listBrowserImage, TAG_END);
	// 	if (listBrowserImage == image) {
	// 		#ifdef __AMIGAOS3__
	// 		DeleteFile(image->filePath);
	// 		#else
	// 		Delete(image->filePath);
	// 		#endif
	// 		// SetGadgetAttrs(imageListBrowser, mainWindow, NULL, LISTBROWSER_Selected, -1, TAG_DONE);
	// 		// SetGadgetAttrs(imageListBrowser, mainWindow, NULL, LISTBROWSER_Labels, ~0, TAG_DONE);
	// 		Remove(node);
	// 		FreeListBrowserNode(node);
	// 		// SetGadgetAttrs(imageListBrowser, mainWindow, NULL, LISTBROWSER_Labels, imageList, TAG_DONE);
	// 		FreeVec(image->filePath);
	// 		FreeVec(image->name);
	// 		FreeVec(image->prompt);
	// 		FreeVec(image);
	// 		return;
	// 	}
	// 	node = node->ln_Succ;
	// }
}

/**
 * The main run loop of the GUI
 * @return The return code of the application
 * @see RETURN_OK
 * @see RETURN_ERROR
**/
LONG startGUIRunLoop() {
	ULONG signals;
	BOOL running = TRUE;
	WORD code;

	refreshSpeechMenuItems();

	while (running) {
		ULONG id = DoMethod(app, MUIM_Application_NewInput, &signals);

		switch(id) {
			case MUIV_Application_ReturnID_Quit:
			case MENU_ITEM_QUIT:
			{
				BOOL forceQuit;
				get(app, MUIA_Application_ForceQuit, &forceQuit);
				if((forceQuit || (app, mainWindowObject, 0, "Quit?", "_Yes|_No", "\33cAre you sure you want to quit AmigaGPT?", 0)) == 1)
						running = FALSE;
				break;
			}
			case MENU_ITEM_ABOUT_AMIGAGPT:
				openAboutAmigaGPTWindow();
				break;
			case MENU_ITEM_ABOUT_MUI:
				openAboutMUIWindow();
				break;
			case MENU_ITEM_VIEW_DOCUMENTATION:
				openDocumentation();
				break;
			default:
				break;
		}
		if(running && signals) Wait(signals);
	}
		// BOOL isTextInputTextEditorActive = GetAttr(GFLG_SELECTED, textInputTextEditor, NULL);
		// BOOL isChatOutputTextEditorActive = GetAttr(GFLG_SELECTED, chatOutputTextEditor, NULL);

		/* while ((result = DoMethod(mainWindowObjectOld, WM_HANDLEINPUT, &code)) != WMHI_LASTMSG) {
			switch (result & WMHI_CLASSMASK) {
				case WMHI_CLOSEWINDOW:
					done = TRUE;
					break;
				case WMHI_GADGETDOWN:
					switch (result & WMHI_GADGETMASK) {
						case TEXT_INPUT_TEXT_EDITOR_ID:
							if (isChatOutputTextEditorActive)
								ActivateLayoutGadget(chatModeLayout, mainWindow, NULL, textInputTextEditor);
								activeTextEditorGadgetID = TEXT_INPUT_TEXT_EDITOR_ID;
							break;
					}
					break;
				case WMHI_GADGETUP:
					switch (result & WMHI_GADGETMASK) {
						case SEND_MESSAGE_BUTTON_ID:
						case TEXT_INPUT_TEXT_EDITOR_ID:
							if (config.openAiApiKey != NULL && strlen(config.openAiApiKey) > 0) {
								sendChatMessage();
								saveConversations();
							}
							else
								displayError("Please enter your OpenAI API key in the Open AI settings in the menu.");
							break;
						case NEW_CHAT_BUTTON_ID:
							currentConversation = NULL;
							DoGadgetMethod(chatOutputTextEditor, mainWindow, NULL, GM_TEXTEDITOR_ClearText, NULL);
							ActivateLayoutGadget(chatModeLayout, mainWindow, NULL, textInputTextEditor);
							activeTextEditorGadgetID = TEXT_INPUT_TEXT_EDITOR_ID;
							break;
						case DELETE_CHAT_BUTTON_ID:
							removeConversationFromConversationList(currentConversation);
							currentConversation = NULL;
							DoGadgetMethod(chatOutputTextEditor, mainWindow, NULL, GM_TEXTEDITOR_ClearText, NULL);
							ActivateLayoutGadget(chatModeLayout, mainWindow, NULL, textInputTextEditor);
							activeTextEditorGadgetID = TEXT_INPUT_TEXT_EDITOR_ID;
							saveConversations();
							break;
						case CONVERSATION_LIST_BROWSER_ID:
							{
								// Switch to the conversation the user clicked on in the list
								struct ListBrowserNode *node;
								GetAttr(LISTBROWSER_SelectedNode, conversationListBrowser, &node);
								struct MinList *conversation;
								GetListBrowserNodeAttrs(node, LBNA_UserData,&conversation, TAG_END);
								currentConversation = conversation;
								displayConversation(currentConversation);
								break;
							}
						case NEW_IMAGE_BUTTON_ID:
							currentImage = NULL;
							SetGadgetAttrs(textInputTextEditor, mainWindow, NULL, GA_ReadOnly, FALSE, TAG_DONE);
							DoGadgetMethod(textInputTextEditor, mainWindow, NULL, GM_TEXTEDITOR_ClearText, NULL);
							SetGadgetAttrs(createImageButton, mainWindow, NULL, GA_Disabled, FALSE, TAG_DONE);
							SetGadgetAttrs(openSmallImageButton, mainWindow, NULL, GA_Disabled, TRUE, TAG_DONE);
							SetGadgetAttrs(openMediumImageButton, mainWindow, NULL, GA_Disabled, TRUE, TAG_DONE);
							SetGadgetAttrs(openLargeImageButton, mainWindow, NULL, GA_Disabled, TRUE, TAG_DONE);
							SetGadgetAttrs(openOriginalImageButton, mainWindow, NULL, GA_Disabled, TRUE, TAG_DONE);
							SetGadgetAttrs(saveCopyButton, mainWindow, NULL, GA_Disabled, TRUE, TAG_DONE);
							ActivateLayoutGadget(imageGenerationModeLayout, mainWindow, NULL, textInputTextEditor);
							break;
						case IMAGE_LIST_BROWSER_ID:
							{
								// Switch to the image the user clicked on in the list
								struct ListBrowserNode *node;
								GetAttr(LISTBROWSER_SelectedNode, imageListBrowser, &node);
								struct GeneratedImage *image;
								GetListBrowserNodeAttrs(node, LBNA_UserData,&image, TAG_END);
								currentImage = image;
								SetGadgetAttrs(textInputTextEditor, mainWindow, NULL, GA_ReadOnly, TRUE, TAG_DONE);
								SetGadgetAttrs(textInputTextEditor, mainWindow, NULL, GA_TEXTEDITOR_Contents, currentImage->prompt, TAG_DONE);
								SetGadgetAttrs(createImageButton, mainWindow, NULL, GA_Disabled, TRUE, TAG_DONE);
								SetGadgetAttrs(openSmallImageButton, mainWindow, NULL, GA_Disabled, FALSE, TAG_DONE);
								SetGadgetAttrs(openMediumImageButton, mainWindow, NULL, GA_Disabled, FALSE, TAG_DONE);
								SetGadgetAttrs(openLargeImageButton, mainWindow, NULL, GA_Disabled, FALSE, TAG_DONE);
								SetGadgetAttrs(openOriginalImageButton, mainWindow, NULL, GA_Disabled, FALSE, TAG_DONE);
								SetGadgetAttrs(saveCopyButton, mainWindow, NULL, GA_Disabled, FALSE, TAG_DONE);
								break;
							}
						case CLICKTAB_MODE_SELECTION_ID:
						{
							struct Node *node;
							GetAttr(CLICKTAB_CurrentNode, modeClickTabOld, &node);
							if (node == chatTabNode) {
								ActivateLayoutGadget(chatModeLayout, mainWindow, NULL, textInputTextEditor);
								SetGadgetAttrs(textInputTextEditor, mainWindow, NULL, GA_ReadOnly, FALSE, TAG_DONE);
								DoGadgetMethod(textInputTextEditor, mainWindow, NULL, GM_TEXTEDITOR_ClearText, NULL);
							} else if (node == imageGenerationTabNode) {
								ActivateLayoutGadget(imageGenerationModeLayout, mainWindow, NULL, textInputTextEditor);
								if (currentImage == NULL) {
									SetGadgetAttrs(textInputTextEditor, mainWindow, NULL, GA_ReadOnly, FALSE, TAG_DONE);
									DoGadgetMethod(textInputTextEditor, mainWindow, NULL, GM_TEXTEDITOR_ClearText, NULL);
								} else {
									SetGadgetAttrs(textInputTextEditor, mainWindow, NULL, GA_ReadOnly, TRUE, TAG_DONE);
									SetGadgetAttrs(textInputTextEditor, mainWindow, NULL, GA_TEXTEDITOR_Contents, currentImage->prompt, TAG_DONE);
								}
							}
							break;
						}
						case CREATE_IMAGE_BUTTON_ID:
							if (config.openAiApiKey != NULL && strlen(config.openAiApiKey) > 0) {
								createImage();
								saveImages();
							} else {
								displayError("Please enter your OpenAI API key in the Open AI settings in the menu.");
							}
							break;
						case DELETE_IMAGE_BUTTON_ID:
							removeImageFromImageList(currentImage);
							currentImage = NULL;
							saveImages();
							break;
						case OPEN_SMALL_IMAGE_BUTTON_ID:
						{
							if (currentImage->width == currentImage->height)
								openImage(currentImage, 256, 256);
							else if (currentImage->width > currentImage->height) {
								LONG height = (currentImage->height * 256) / currentImage->width;
								openImage(currentImage, 256, height);
							} else {
								LONG width = (currentImage->width * 256) / currentImage->height;
								openImage(currentImage, width, 256);
							}
							break;
						}
						case OPEN_MEDIUM_IMAGE_BUTTON_ID:
						{
							if (currentImage->width == currentImage->height)
								openImage(currentImage, 512, 512);
							else if (currentImage->width > currentImage->height) {
								LONG height = (currentImage->height * 512) / currentImage->width;
								openImage(currentImage, 512, height);
							} else {
								LONG width = (currentImage->width * 512) / currentImage->height;
								openImage(currentImage, width, 512);
							}
							break;
						}
						case OPEN_LARGE_IMAGE_BUTTON_ID:
						{
							if (currentImage->width == currentImage->height)
								openImage(currentImage, 1024, 1024);
							else if (currentImage->width > currentImage->height) {
								LONG height = (currentImage->height * 1024) / currentImage->width;
								openImage(currentImage, 1024, height);
							} else {
								LONG width = (currentImage->width * 1024) / currentImage->height;
								openImage(currentImage, width, 1024);
							}
							break;
						}
						case OPEN_ORIGINAL_IMAGE_BUTTON_ID:
							openImage(currentImage, currentImage->width, currentImage->height);
							break;
						case SAVE_COPY_BUTTON_ID:
							saveImageCopy(currentImage);
							break;
					}
					break;
				case WMHI_MENUPICK:
				{
					struct MenuItem *menuItem = ItemAddress(menu, code);
					ULONG itemIndex = code != -1 ? (ULONG)GTMENUITEM_USERDATA(menuItem) : NULL_ID;
					switch (itemIndex) {
						case MENU_ITEM_ABOUT_ID:
							openAboutWindow();
							break;
						case MENU_ITEM_CUT_ID:
						{
							switch (activeTextEditorGadgetID) {
								case TEXT_INPUT_TEXT_EDITOR_ID:
									DoGadgetMethod(textInputTextEditor, mainWindow, NULL, GM_TEXTEDITOR_ARexxCmd, NULL, "CUT");
									break;
								case CHAT_OUTPUT_TEXT_EDITOR_ID:
									DoGadgetMethod(chatOutputTextEditor, mainWindow, NULL, GM_TEXTEDITOR_ARexxCmd, NULL, "COPY");
									break;
							}
						}
						case MENU_ITEM_COPY_ID:
						{
							switch (activeTextEditorGadgetID) {
								case TEXT_INPUT_TEXT_EDITOR_ID:
									DoGadgetMethod(textInputTextEditor, mainWindow, NULL, GM_TEXTEDITOR_ARexxCmd, NULL, "COPY");
									break;
								case CHAT_OUTPUT_TEXT_EDITOR_ID:
									DoGadgetMethod(chatOutputTextEditor, mainWindow, NULL, GM_TEXTEDITOR_ARexxCmd, NULL, "COPY");
									break;
							}
							break;
						}
						case MENU_ITEM_PASTE_ID:
						{
							DoGadgetMethod(textInputTextEditor, mainWindow, NULL, GM_TEXTEDITOR_ARexxCmd, NULL, "PASTE");
							break;
						}
						case MENU_ITEM_SELECT_ALL_ID:
						{
							switch (activeTextEditorGadgetID) {
								case TEXT_INPUT_TEXT_EDITOR_ID:
									DoGadgetMethod(textInputTextEditor, mainWindow, NULL, GM_TEXTEDITOR_MarkText, NULL, 0, 0, 65535, 65535);
									break;
								case CHAT_OUTPUT_TEXT_EDITOR_ID:
									DoGadgetMethod(chatOutputTextEditor, mainWindow, NULL, GM_TEXTEDITOR_MarkText, NULL, 0, 0, 65535, 65535);
									break;
							}
							break;
						}
						case MENU_ITEM_CLEAR_ID:
						{
							DoGadgetMethod(textInputTextEditor, mainWindow, NULL, GM_TEXTEDITOR_ARexxCmd, NULL, "CLEAR");
							break;
						}
						case MENU_ITEM_QUIT_ID:
							done = TRUE;
							break;
						case MENU_ITEM_CHAT_FONT_ID:
							openChatFontRequester();
							break;
						case MENU_ITEM_UI_FONT_ID:
							openUIFontRequester();
							break;
						case MENU_ITEM_SPEECH_ENABLED_ID:
							config.speechEnabled = !config.speechEnabled;
							if (config.speechEnabled) {
								closeSpeech();
								if (initSpeech(config.speechSystem) == RETURN_ERROR) {
									displayError("Could not initialise selected speech system.");
								}
							}
							writeConfig();
							refreshSpeechMenuItems();
							break;
						#ifdef __AMIGAOS3__
						case MENU_ITEM_SPEECH_ACCENT_ID:
							openSpeechAccentRequester();
							break;
						case MENU_ITEM_SPEECH_SYSTEM_34_ID:
							closeSpeech();
							config.speechSystem = SPEECH_SYSTEM_34;
							if (initSpeech(SPEECH_SYSTEM_34) == RETURN_ERROR) {
								displayError("Could not initialise speech system v34. Please make sure the translator.library and narrator.device v34 are installed into the program directory.");								
							}
							writeConfig();
							refreshSpeechMenuItems();
							break;
						case MENU_ITEM_SPEECH_SYSTEM_37_ID:
							closeSpeech();
							config.speechSystem = SPEECH_SYSTEM_37;
							if (initSpeech(SPEECH_SYSTEM_37) == RETURN_ERROR) {
								displayError("Could not initialise speech system v37. Please make sure the translator.library and narrator.device v37 are installed into the program directory.");
							}
							writeConfig();
							refreshSpeechMenuItems();
							break;
						#else
						case MENU_ITEM_SPEECH_SYSTEM_FLITE_ID:
							closeSpeech();
							config.speechSystem = SPEECH_SYSTEM_FLITE;
							if (initSpeech(SPEECH_SYSTEM_FLITE) == RETURN_ERROR) {
								displayError("Could not initialise speech system Flite. Please make sure the flite.device is installed into the program directory.");								
							}
							writeConfig();
							refreshSpeechMenuItems();
							break;
						#endif
						case MENU_ITEM_SPEECH_SYSTEM_OPENAI_ID:
							closeSpeech();
							initSpeech(SPEECH_SYSTEM_OPENAI);
							config.speechSystem = SPEECH_SYSTEM_OPENAI;
							writeConfig();
							refreshSpeechMenuItems();
							break;
						#ifdef __AMIGAOS4__
						case MENU_ITEM_SPEECH_FLITE_VOICE_AWB_ID:
							config.speechFliteVoice = SPEECH_FLITE_VOICE_AWB;
							writeConfig();
							refreshSpeechMenuItems();
							break;
						case MENU_ITEM_SPEECH_FLITE_VOICE_KAL_ID:
							config.speechFliteVoice = SPEECH_FLITE_VOICE_KAL;
							writeConfig();
							refreshSpeechMenuItems();
							break;
						case MENU_ITEM_SPEECH_FLITE_VOICE_KAL16_ID:
							config.speechFliteVoice = SPEECH_FLITE_VOICE_KAL16;
							writeConfig();
							refreshSpeechMenuItems();
							break;
						case MENU_ITEM_SPEECH_FLITE_VOICE_RMS_ID:
							config.speechFliteVoice = SPEECH_FLITE_VOICE_RMS;
							writeConfig();
							refreshSpeechMenuItems();
							break;
						case MENU_ITEM_SPEECH_FLITE_VOICE_SLT_ID:
							config.speechFliteVoice = SPEECH_FLITE_VOICE_SLT;
							writeConfig();
							refreshSpeechMenuItems();
							break;
						#endif
						case MENU_ITEM_SPEECH_OPENAI_MODEL_TTS_1_ID:
							config.openAITTSModel = OPENAI_TTS_MODEL_TTS_1;
							writeConfig();
							refreshSpeechMenuItems();
							break;
						case MENU_ITEM_SPEECH_OPENAI_MODEL_TTS_1_HD_ID:
							config.openAITTSModel = OPENAI_TTS_MODEL_TTS_1_HD;
							writeConfig();
							refreshSpeechMenuItems();
							break;
						case MENU_ITEM_SPEECH_OPENAI_VOICE_ALLOY_ID:
							config.openAITTSVoice = OPENAI_TTS_VOICE_ALLOY;
							writeConfig();
							refreshSpeechMenuItems();
							break;
						case MENU_ITEM_SPEECH_OPENAI_VOICE_ECHO_ID:
							config.openAITTSVoice = OPENAI_TTS_VOICE_ECHO;
							writeConfig();
							refreshSpeechMenuItems();
							break;
						case MENU_ITEM_SPEECH_OPENAI_VOICE_FABLE_ID:
							config.openAITTSVoice = OPENAI_TTS_VOICE_FABLE;
							writeConfig();
							refreshSpeechMenuItems();
							break;
						case MENU_ITEM_SPEECH_OPENAI_VOICE_ONYX_ID:
							config.openAITTSVoice = OPENAI_TTS_VOICE_ONYX;
							writeConfig();
							refreshSpeechMenuItems();
							break;
						case MENU_ITEM_SPEECH_OPENAI_VOICE_NOVA_ID:
							config.openAITTSVoice = OPENAI_TTS_VOICE_NOVA;
							writeConfig();
							refreshSpeechMenuItems();
							break;
						case MENU_ITEM_SPEECH_OPENAI_VOICE_SHIMMER_ID:
							config.openAITTSVoice = OPENAI_TTS_VOICE_SHIMMER;
							writeConfig();
							refreshSpeechMenuItems();
							break;
						case MENU_ITEM_OPENAI_API_KEY_ID:
							openApiKeyRequester();
							break;
						case MENU_ITEM_CHAT_SYSTEM_ID:
							openChatSystemRequester();
							break;
						case MENU_ITEM_CHAT_MODEL_GPT_4o_ID:
							config.chatModel = GPT_4o;
							writeConfig();
							refreshOpenAIMenuItems();
							break;
						case MENU_ITEM_CHAT_MODEL_GPT_4o_2024_05_13_ID:
							config.chatModel = GPT_4o_2024_05_13;
							writeConfig();
							refreshOpenAIMenuItems();
							break;
						case MENU_ITEM_CHAT_MODEL_GPT_4o_MINI_ID:
							config.chatModel = GPT_4o_MINI;
							writeConfig();
							refreshOpenAIMenuItems();
							break;
						case MENU_ITEM_CHAT_MODEL_GPT_4o_MINI_2024_07_18_ID:
							config.chatModel = GPT_4o_MINI_2024_07_18;
							writeConfig();
							refreshOpenAIMenuItems();
							break;
						case MENU_ITEM_CHAT_MODEL_GPT_4_TURBO_ID:
							config.chatModel = GPT_4_TURBO;
							writeConfig();
							refreshOpenAIMenuItems();
							break;
						case MENU_ITEM_CHAT_MODEL_GPT_4_TURBO_PREVIEW_ID:
							config.chatModel = GPT_4_TURBO_PREVIEW;
							writeConfig();
							refreshOpenAIMenuItems();
							break;
						case MENU_ITEM_CHAT_MODEL_GPT_4_0125_PREVIEW_ID:
							config.chatModel = GPT_4_0125_PREVIEW;
							writeConfig();
							refreshOpenAIMenuItems();
							break;
						case MENU_ITEM_CHAT_MODEL_GPT_4_1106_PREVIEW_ID:
							config.chatModel = GPT_4_1106_PREVIEW;
							writeConfig();
							refreshOpenAIMenuItems();
							break;
						case MENU_ITEM_CHAT_MODEL_GPT_4_ID:
							config.chatModel = GPT_4;
							writeConfig();
							refreshOpenAIMenuItems();
							break;
						case MENU_ITEM_CHAT_MODEL_GPT_4_0613_ID:
							config.chatModel = GPT_4_0613;
							writeConfig();
							refreshOpenAIMenuItems();
							break;
						case MENU_ITEM_CHAT_MODEL_GPT_3_5_TURBO_ID:
							config.chatModel = GPT_3_5_TURBO;
							writeConfig();
							refreshOpenAIMenuItems();
							break;
						case MENU_ITEM_CHAT_MODEL_GPT_3_5_TURBO_0125_ID:
							config.chatModel = GPT_3_5_TURBO_0125;
							writeConfig();
							refreshOpenAIMenuItems();
							break;
						case MENU_ITEM_CHAT_MODEL_GPT_3_5_TURBO_1106_ID:
							config.chatModel = GPT_3_5_TURBO_1106;
							writeConfig();
							refreshOpenAIMenuItems();
							break;
						case MENU_ITEM_IMAGE_MODEL_DALL_E_2_ID:
							config.imageModel = DALL_E_2;
							writeConfig();
							refreshOpenAIMenuItems();
							break;
						case MENU_ITEM_IMAGE_MODEL_DALL_E_3_ID:
							config.imageModel = DALL_E_3;
							writeConfig();
							refreshOpenAIMenuItems();
							break;
						case MENU_ITEM_IMAGE_SIZE_DALL_E_2_256X256_ID:
							config.imageSizeDallE2 = IMAGE_SIZE_256x256;
							writeConfig();
							refreshOpenAIMenuItems();
							break;
						case MENU_ITEM_IMAGE_SIZE_DALL_E_2_512X512_ID:
							config.imageSizeDallE2 = IMAGE_SIZE_512x512;
							writeConfig();
							refreshOpenAIMenuItems();
							break;
						case MENU_ITEM_IMAGE_SIZE_DALL_E_2_1024X1024_ID:
							config.imageSizeDallE2 = IMAGE_SIZE_1024x1024;
							writeConfig();
							refreshOpenAIMenuItems();
							break;
						case MENU_ITEM_IMAGE_SIZE_DALL_E_3_1024X1024_ID:
							config.imageSizeDallE3 = IMAGE_SIZE_1024x1024;
							writeConfig();
							refreshOpenAIMenuItems();
							break;
						case MENU_ITEM_IMAGE_SIZE_DALL_E_3_1792X1024_ID:
							config.imageSizeDallE3 = IMAGE_SIZE_1792x1024;
							writeConfig();
							refreshOpenAIMenuItems();
							break;
						case MENU_ITEM_IMAGE_SIZE_DALL_E_3_1024X1792_ID:
							config.imageSizeDallE3 = IMAGE_SIZE_1024x1792;
							writeConfig();
							refreshOpenAIMenuItems();
							break;
						case MENU_ITEM_VIEW_DOCUMENTATION_ID:
							openDocumentation();
							break;
						default:
							break;
					}
					break;
				}
				case WMHI_RAWKEY:
					switch (code) {
						case HELP_KEY:
							openDocumentation();
							break;
					}
					break;
				default:
					break;
			}
		}
	}
	*/

	saveConversations();
	saveImages();

	return RETURN_OK;
}

/**
 * Opens the application's documentation guide
*/
void openDocumentation() {
	struct NewAmigaGuide guide = {
		.nag_Name = PROGDIR"AmigaGPT.guide",
		.nag_Screen = screen,
		.nag_PubScreen = NULL,
		.nag_BaseName = "AmigaGPT",
		.nag_Extens = NULL,
	};
	AMIGAGUIDECONTEXT handle;
	if (handle = OpenAmigaGuide(&guide, NULL)) {
		CloseAmigaGuide(handle);
	} else {
		displayDiskError("Could not open documentation", IoErr());
	}
}

/**
 * Display an error message
 * @param message the message to display
**/
void displayError(STRPTR message) {
	DisplayBeep(screen);
	updateStatusBar("Error", 6);

	STRPTR adjustedMsg = AllocVec(strlen(message) + 200, MEMF_ANY | MEMF_CLEAR);
	STRPTR dest = adjustedMsg;

	ULONG width = 300;
	struct RastPort rp = screen->RastPort;
	struct TextExtent te;

	while (*message != '\0') {
		// Find how much of the message fits within half the screen width
		ULONG fits = TextFit(&rp, message, strlen(message), &te, NULL, 1, width, rp.Font->tf_YSize);

		// Find last space in the range that fits
		STRPTR lastSpace = message + fits;
		while (lastSpace > message && *lastSpace != ' ') {
			lastSpace--;
		}

		// If we found a space, replace it with a newline and copy the line to the output
		if (*lastSpace == ' ' && lastSpace != message) {
			memcpy(dest, message, lastSpace - message);
			dest[lastSpace - message] = '\n';
			dest += lastSpace - message + 1;
			message = lastSpace + 1;
		} else {
			// If we didn't find a space, just copy the part that fits
			memcpy(dest, message, fits);
			dest += fits;
			message += fits;
			// If we have not reached the end of the message, add a newline
			if (*message != '\0') {
				*dest++ = '\n';
			}
		}
	}

	*dest = '\0';

	struct EasyStruct errorRequester = {
		sizeof(struct EasyStruct),
		0,
		"Error",
		adjustedMsg,
		 "OK"
	};
	EasyRequest(mainWindow, &errorRequester, NULL, NULL);

	set(sendMessageButton, MUIA_Disabled, FALSE);
	FreeVec(adjustedMsg);
}

/**
 * Display an error message about a disk error
 * @param message the message to display
 * @param error the error code returned by IOErr()
**/
void displayDiskError(STRPTR message, LONG error) {
	const UBYTE ERROR_BUFFER_LENGTH = 255;
	const UBYTE FINAL_MESSAGE_LENGTH = strlen(message) + ERROR_BUFFER_LENGTH;
	STRPTR errorMessage = AllocVec(ERROR_BUFFER_LENGTH, MEMF_ANY | MEMF_CLEAR);
	STRPTR finalMessage = AllocVec(FINAL_MESSAGE_LENGTH, MEMF_ANY | MEMF_CLEAR);
	Fault(error, NULL, errorMessage, ERROR_BUFFER_LENGTH);
	snprintf(finalMessage, FINAL_MESSAGE_LENGTH - 3, "%s\n\n%s", message, error);
	displayError(finalMessage);
	FreeVec(errorMessage);
	FreeVec(finalMessage);
}

/**
 * Opens the About AmigaGPT window
**/
static void openAboutAmigaGPTWindow() {
	if(aboutAmigaGPTWindowObject) {
	  set(aboutAmigaGPTWindowObject, MUIA_Window_Open, TRUE);
	} else {
	  struct EasyStruct aboutRequester = {
		sizeof(struct EasyStruct),
		0,
		"About",
		#ifdef __AMIGAOS3__
		"AmigaGPT for m68k AmigaOS 3\n\n"
		#else
		"AmigaGPT for PPC AmigaOS 4\n\n"
		#endif
		"Version " APP_VERSION "\n"
		"Build date: " __DATE__ "\n"
		"Build number: " BUILD_NUMBER "\n\n"
		"Developed by Cameron Armstrong (@sacredbanana on GitHub,\n"
		"YouTube and Twitter, @Nightfox on EAB)\n\n"
		"This app will always remain free but if you would like to\n"
		"support me you can do so at https://paypal.me/sacredbanana",
		"OK"
	};
	ULONG flags = IDCMP_RAWKEY | IDCMP_MOUSEBUTTONS;
	EasyRequest(mainWindow, &aboutRequester, &flags, NULL);
	}
}

/**
 * Opens the About NUI window
**/
static void openAboutMUIWindow() {
	if(aboutMUIWindowObject == NULL) {
	  aboutMUIWindowObject = AboutmuiObject,
	    MUIA_Window_RefWindow, mainWindowObject,
	    MUIA_Aboutmui_Application, app,
	    End;
	}

	if(aboutMUIWindowObject != NULL)
	  set(aboutMUIWindowObject, MUIA_Window_Open, TRUE);
	else
	  DisplayBeep(0);
}

/**
 * Opens a requester for the user to select the font for the chat window
**/
static void openChatFontRequester() {
	struct FontRequester *fontRequester;
	if (fontRequester = (struct FontRequester *)AllocAslRequestTags(ASL_FontRequest, TAG_DONE)) {
		struct TextAttr *chatFont;
		if (AslRequestTags(fontRequester, ASLFO_Window, (ULONG)mainWindow, TAG_DONE)) {
			if (config.chatFontName != NULL) {
				FreeVec(config.chatFontName);
			}
			chatFont = &fontRequester->fo_Attr;
			config.chatFontName = AllocVec(strlen(chatFont->ta_Name) + 1, MEMF_ANY | MEMF_CLEAR);
			strncpy(config.chatFontName, chatFont->ta_Name, strlen(chatFont->ta_Name));
			config.chatFontSize = chatFont->ta_YSize;
			config.chatFontStyle = chatFont->ta_Style;
			config.chatFontFlags = chatFont->ta_Flags;
			writeConfig();
			printf("Chat font name: %s\n", config.chatFontName);
			// SetGadgetAttrs(textInputTextEditor, mainWindow, NULL, GA_TextAttr, chatFont, TAG_DONE);
			// SetGadgetAttrs(chatOutputTextEditor, mainWindow, NULL, GA_TextAttr, chatFont, TAG_DONE);
		}
		FreeAslRequest(fontRequester);
	}
}

/**
 * Opens a requester for the user to select the font for the UI
**/
static void openUIFontRequester() {
	struct FontRequester *fontRequester;
	// GetAttr(WINDOW_Window, mainWindowObjectOld, &mainWindow);
	if (fontRequester = (struct FontRequester *)AllocAslRequestTags(ASL_FontRequest, TAG_DONE)) {
		struct TextAttr *uiFont;
		if (AslRequestTags(fontRequester, ASLFO_Window, (ULONG)mainWindow, TAG_DONE)) {
			if (config.uiFontName != NULL) {
				FreeVec(config.uiFontName);
			}
			uiFont = &fontRequester->fo_Attr;
			config.uiFontName = AllocVec(strlen(uiFont->ta_Name) + 1, MEMF_ANY | MEMF_CLEAR);
			strncpy(config.uiFontName, uiFont->ta_Name, strlen(uiFont->ta_Name));
			config.uiFontSize = uiFont->ta_YSize;
			config.uiFontStyle = uiFont->ta_Style;
			config.uiFontFlags = uiFont->ta_Flags;
			writeConfig();
			if (!isPublicScreen) {
				if (uiTextFont)
					CloseFont(uiTextFont);

				// DoMethod(mainWindowObjectOld, WM_CLOSE, NULL);
				uiTextFont = OpenFont(uiFont);
				SetFont(&(screen->RastPort), uiTextFont);
				SetFont(mainWindow->RPort, uiTextFont);
				RemakeDisplay();
				RethinkDisplay();
				// mainWindow = DoMethod(mainWindowObjectOld, WM_OPEN, NULL);
			}
			// SetGadgetAttrs(newChatButton, mainWindow, NULL, GA_TextAttr, uiFont, TAG_DONE);
			// SetGadgetAttrs(deleteChatButton, mainWindow, NULL, GA_TextAttr, uiFont, TAG_DONE);
			// SetGadgetAttrs(sendMessageButton, mainWindow, NULL, GA_TextAttr, uiFont, TAG_DONE);
			// SetGadgetAttrs(statusBar, mainWindow, NULL, GA_TextAttr, uiFont, TAG_DONE);
			updateStatusBar("Ready", 5);
			// SetGadgetAttrs(conversationListBrowser, mainWindow, NULL, GA_TextAttr, uiFont, TAG_DONE);

		}
		FreeAslRequest(fontRequester);
	}
}


/**
 * Opens a requester for the user to select accent for the speech
**/
static void openSpeechAccentRequester() {
	#ifdef __AMIGAOS3__
	struct FileRequester *fileRequester = (struct FileRequester *)AllocAslRequestTags(
									ASL_FileRequest,
									ASLFR_Window, mainWindow,
									ASLFR_PopToFront, TRUE,
									ASLFR_Activate, TRUE,
									ASLFR_DrawersOnly, FALSE,
									ASLFR_InitialDrawer, "LOCALE:accents",
									ASLFR_DoPatterns, TRUE,
									ASLFR_InitialPattern, "#?.accent",
									TAG_DONE);

	if (fileRequester) {
		if (AslRequestTags(fileRequester, TAG_DONE)) {
			if (config.speechAccent != NULL) {
				FreeVec(config.speechAccent);
			}
			config.speechAccent = AllocVec(strlen(fileRequester->fr_File) + 1, MEMF_ANY | MEMF_CLEAR);
			strncpy(config.speechAccent, fileRequester->fr_File, strlen(fileRequester->fr_File));
			writeConfig();
		}
		FreeAslRequest(fileRequester);
	}
	#endif
}

/**
 * Opens a requester for the user to enter their OpenAI API key
**/
static void openApiKeyRequester() {
	// STRPTR buffer = AllocVec(OPENAI_API_KEY_LENGTH + 1, MEMF_ANY | MEMF_CLEAR);
	// if (buffer == NULL) {
	// 	displayError("Failed to allocate memory for API key buffer");
	// 	return;
	// }
	// if (config.openAiApiKey != NULL) {
	// 	strncpy(buffer, config.openAiApiKey, OPENAI_API_KEY_LENGTH);
	// }
	// Object *apiKeyRequester = NewObject(REQUESTER_GetClass(), NULL,
	// 	REQ_Type, REQTYPE_STRING,
	// 	REQ_TitleText, "Enter your OpenAI API key",
	// 	REQ_BodyText, "Please type or paste (Right Amiga + V) your OpenAI API key here",
	// 	REQ_GadgetText, "OK|Cancel",
	// 	REQ_Image, REQIMAGE_INFO,
	// 	REQS_AllowEmpty, FALSE,
	// 	REQS_Buffer, buffer,
	// 	REQS_MaxChars, OPENAI_API_KEY_LENGTH,
	// 	REQ_ForceFocus, TRUE,
	// 	TAG_DONE);

	// if (apiKeyRequester) {
	// 	ULONG result = OpenRequester(apiKeyRequester, mainWindow);
	// 	if (result == 1) {
	// 		if (config.openAiApiKey != NULL) {
	// 			FreeVec(config.openAiApiKey);
	// 		}
	// 		config.openAiApiKey = AllocVec(strlen(buffer) + 1, MEMF_ANY | MEMF_CLEAR);
	// 		strncpy(config.openAiApiKey, buffer, strlen(buffer));
	// 		writeConfig();
	// 	}
	// 	DisposeObject(apiKeyRequester);
	// }
	// FreeVec(buffer);
}

/**
 * Opens a requester for the user to enter the chat system
**/
static void openChatSystemRequester() {
	// STRPTR buffer = AllocVec(CHAT_SYSTEM_LENGTH + 1, MEMF_ANY | MEMF_CLEAR);
	// if (buffer == NULL) {
	// 	displayError("Failed to allocate memory for chat system buffer");
	// 	return;
	// }
	// if (config.chatSystem != NULL) {
	// 	strncpy(buffer, config.chatSystem, CHAT_SYSTEM_LENGTH);
	// }
	// Object *chatSystemRequester = NewObject(REQUESTER_GetClass(), NULL,
	// 	REQ_Type, REQTYPE_STRING,
	// 	REQ_TitleText, "Enter how you would like AmigaGPT to respond",
	// 	REQ_BodyText, "If you would like AmigaGPT to respond in a\n"
	// 				  "certain way, or add some personality, type\n"
	// 				  "it in here. For example, you could type\n" 
	// 				  "\"Speak like a pirate\" or \"Speak like a\n"
	// 				  "robot\". If you would like AmigaGPT to respond\n"
	// 				  "normally, leave this blank. This setting will\n"
	// 				  "be applied to new conversations only.",
	// 	REQ_GadgetText, "OK|Cancel",
	// 	REQ_Image, REQIMAGE_INFO,
	// 	REQS_AllowEmpty, TRUE,
	// 	REQS_Buffer, buffer,
	// 	REQS_MaxChars, CHAT_SYSTEM_LENGTH,
	// 	REQ_ForceFocus, TRUE,
	// 	TAG_DONE);

	// if (chatSystemRequester) {
	// 	ULONG result = OpenRequester(chatSystemRequester, mainWindow);
	// 	if (result == 1) {
	// 		if (config.chatSystem != NULL) {
	// 			FreeVec(config.chatSystem);
	// 		}
	// 		config.chatSystem = AllocVec(strlen(buffer) + 1, MEMF_ANY | MEMF_CLEAR);
	// 		strncpy(config.chatSystem, buffer, strlen(buffer));
	// 		writeConfig();
	// 	}
	// 	DisposeObject(chatSystemRequester);
	// }
	// FreeVec(buffer);
}

/**
 * Saves the conversations to disk
 * @return RETURN_OK on success, RETURN_ERROR on failure
**/
LONG saveConversations() {
	// BPTR file = Open(PROGDIR"chat-history.json", MODE_NEWFILE);
	// if (file == 0) {
	// 	displayDiskError("Failed to create message history file. Conversation history will not be saved.", IoErr());
	// 	return RETURN_ERROR;
	// }

	// struct json_object *conversationsJsonArray = json_object_new_array();
	// struct json_object *conversationJsonObject;
	// struct Node *conversationListNode = conversationList->lh_Head;
	// while (conversationListNode->ln_Succ) {
	// 	conversationJsonObject = json_object_new_object();
	// 	struct MinList *conversation;
	// 	GetListBrowserNodeAttrs(conversationListNode, LBNA_UserData, &conversation, TAG_END);
	// 	STRPTR conversationTitle;
	// 	GetListBrowserNodeAttrs(conversationListNode, LBNCA_Text, &conversationTitle, TAG_END);
	// 	json_object_object_add(conversationJsonObject, "name", json_object_new_string(conversationTitle));
	// 	struct json_object *messagesJsonArray = json_object_new_array();
	// 	struct ConversationNode *conversationNode;
	// 	for (conversationNode = (struct ConversationNode *)conversation->mlh_Head;
	// 		conversationNode->node.mln_Succ != NULL;
	// 		conversationNode = (struct ConversationNode *)conversationNode->node.mln_Succ) {
	// 		if (!strcmp(conversationNode->role, "system")) continue;
	// 		struct json_object *messageJsonObject = json_object_new_object();
	// 		json_object_object_add(messageJsonObject, "role", json_object_new_string(conversationNode->role));
	// 		json_object_object_add(messageJsonObject, "content", json_object_new_string(conversationNode->content));
	// 		json_object_array_add(messagesJsonArray, messageJsonObject);
	// 	}
	// 	json_object_object_add(conversationJsonObject, "messages", messagesJsonArray);
	// 	json_object_array_add(conversationsJsonArray, conversationJsonObject);
	// 	conversationListNode = conversationListNode->ln_Succ;
	// }

	// STRPTR conversationsJsonString = (STRPTR)json_object_to_json_string_ext(conversationsJsonArray, JSON_C_TO_STRING_PRETTY);

	// if (Write(file, conversationsJsonString, strlen(conversationsJsonString)) != (LONG)strlen(conversationsJsonString)) {
	// 	displayError("Failed to write to message history file. Conversation history will not be saved.");
	// 	Close(file);
	// 	json_object_put(conversationsJsonArray);
	// 	return RETURN_ERROR;
	// }

	// Close(file);
	// json_object_put(conversationsJsonArray);
	return RETURN_OK;
}

/**
 * Saves the images to disk
 * @return RETURN_OK on success, RETURN_ERROR on failure
**/
static LONG saveImages() {
	// BPTR file = Open(PROGDIR"image-history.json", MODE_NEWFILE);
	// if (file == 0) {
	// 	displayDiskError("Failed to create image history file. Image history will not be saved.", IoErr());
	// 	return RETURN_ERROR;
	// }

	// struct json_object *imagesJsonArray = json_object_new_array();
	// struct json_object *imageJsonObject;
	// struct Node *imageListNode = imageList->lh_Head;
	// while (imageListNode->ln_Succ) {
	// 	imageJsonObject = json_object_new_object();
	// 	struct GeneratedImage *generatedImage;
	// 	GetListBrowserNodeAttrs(imageListNode, LBNA_UserData, &generatedImage, TAG_END);
	// 	json_object_object_add(imageJsonObject, "name", json_object_new_string(generatedImage->name));
	// 	json_object_object_add(imageJsonObject, "filePath", json_object_new_string(generatedImage->filePath));
	// 	json_object_object_add(imageJsonObject, "prompt", json_object_new_string(generatedImage->prompt));
	// 	json_object_object_add(imageJsonObject, "imageModel", json_object_new_int(generatedImage->imageModel));
	// 	json_object_object_add(imageJsonObject, "width", json_object_new_int(generatedImage->width));
	// 	json_object_object_add(imageJsonObject, "height", json_object_new_int(generatedImage->height));
	// 	json_object_array_add(imagesJsonArray, imageJsonObject);
	// 	imageListNode = imageListNode->ln_Succ;
	// }

	// STRPTR imagesJsonString = (STRPTR)json_object_to_json_string_ext(imagesJsonArray, JSON_C_TO_STRING_PRETTY);

	// if (Write(file, imagesJsonString, strlen(imagesJsonString)) != (LONG)strlen(imagesJsonString)) {
	// 	displayError("Failed to write to image history file. Image history will not be saved.");
	// 	Close(file);
	// 	json_object_put(imagesJsonArray);
	// 	return RETURN_ERROR;
	// }

	// Close(file);
	// json_object_put(imagesJsonArray);
	return RETURN_OK;
}

static void createImage() {
	struct json_object *response;

	// SetGadgetAttrs(createImageButton, mainWindow, NULL, GA_Disabled, TRUE, TAG_DONE);
	// SetGadgetAttrs(openSmallImageButton, mainWindow, NULL, GA_Disabled, TRUE, TAG_DONE);
	// SetGadgetAttrs(openMediumImageButton, mainWindow, NULL, GA_Disabled, TRUE, TAG_DONE);
	// SetGadgetAttrs(openLargeImageButton, mainWindow, NULL, GA_Disabled, TRUE, TAG_DONE);
	// SetGadgetAttrs(openOriginalImageButton, mainWindow, NULL, GA_Disabled, TRUE, TAG_DONE);
	// SetGadgetAttrs(saveCopyButton, mainWindow, NULL, GA_Disabled, TRUE, TAG_DONE);
	// SetGadgetAttrs(newImageButton, mainWindow, NULL, GA_Disabled, TRUE, TAG_DONE);
	// SetGadgetAttrs(deleteImageButton, mainWindow, NULL, GA_Disabled, TRUE, TAG_DONE);
	// SetGadgetAttrs(textInputTextEditor, mainWindow, NULL, GA_Disabled, TRUE, TAG_DONE);

	// STRPTR text = DoGadgetMethod(textInputTextEditor, mainWindow, NULL, GM_TEXTEDITOR_ExportText, NULL);

	// // Remove trailing newline characters
	// while (text[strlen(text) - 1] == '\n') {
	// 	text[strlen(text) - 1] = '\0';
	// }
	// STRPTR textUTF_8 = ISO8859_1ToUTF8(text);

	// const enum ImageSize imageSize = config.imageModel == DALL_E_2 ? config.imageSizeDallE2 : config.imageSizeDallE3;
	// response = postImageCreationRequestToOpenAI(textUTF_8, config.imageModel, imageSize, config.openAiApiKey);
	// if (response == NULL) {
	// 	displayError("Error connecting. Please try again.");
	// 	SetGadgetAttrs(createImageButton, mainWindow, NULL, GA_Disabled, FALSE, TAG_DONE);
	// 	SetGadgetAttrs(newImageButton, mainWindow, NULL, GA_Disabled, FALSE, TAG_DONE);
	// 	SetGadgetAttrs(deleteImageButton, mainWindow, NULL, GA_Disabled, FALSE, TAG_DONE);
	// 	SetGadgetAttrs(textInputTextEditor, mainWindow, NULL, GA_Disabled, FALSE, TAG_DONE);
	// 	updateStatusBar("Error", 6);
	// 	return;
	// }
	// struct json_object *error;

	// if (json_object_object_get_ex(response, "error", &error)) {
	// 	struct json_object *message = json_object_object_get(error, "message");
	// 	STRPTR messageString = json_object_get_string(message);
	// 	displayError(messageString);
	// 	SetGadgetAttrs(textInputTextEditor, mainWindow, NULL, GA_TEXTEDITOR_Contents, text, TAG_DONE);
	// 	json_object_put(response);
	// 	SetGadgetAttrs(createImageButton, mainWindow, NULL, GA_Disabled, FALSE, TAG_DONE);
	// 	SetGadgetAttrs(newImageButton, mainWindow, NULL, GA_Disabled, FALSE, TAG_DONE);
	// 	SetGadgetAttrs(deleteImageButton, mainWindow, NULL, GA_Disabled, FALSE, TAG_DONE);
	// 	SetGadgetAttrs(textInputTextEditor, mainWindow, NULL, GA_Disabled, FALSE, TAG_DONE);
	// 	updateStatusBar("Error", 6);
	// 	json_object_put(response);
	// 	return;
	// }

	// struct array_list *data = json_object_get_array(json_object_object_get(response, "data"));
	// struct json_object *dataObject = (struct json_object *)data->array[0];

	// STRPTR url = json_object_get_string(json_object_object_get(dataObject, "url"));

	// CreateDir(PROGDIR"images");

	// // Generate unique ID for the image
	// UBYTE fullPath[30] = "";
	// UBYTE id[11] = "";
	// CONST_STRPTR idChars = "abcdefghijklmnopqrstuvwxyz0123456789";
	// srand(time(NULL));
	// for (int i = 0; i < 9; i++) {
	// 	id[i] = idChars[rand() % strlen(idChars)];
	// }
	// snprintf(fullPath, sizeof(fullPath), PROGDIR"images/%s.png", id);

	// downloadFile(url, fullPath);

	// json_object_put(response);

	// WORD imageWidth, imageHeight;
	// switch (imageSize) {
	// 	case IMAGE_SIZE_256x256:
	// 		imageWidth = 256;
	// 		imageHeight = 256;
	// 		break;
	// 	case IMAGE_SIZE_512x512:
	// 		imageWidth = 512;
	// 		imageHeight = 512;
	// 		break;
	// 	case IMAGE_SIZE_1024x1024:
	// 		imageWidth = 1024;
	// 		imageHeight = 1024;
	// 		break;
	// 	case IMAGE_SIZE_1792x1024:
	// 		imageWidth = 1792;
	// 		imageHeight = 1024;
	// 		break;
	// 	case IMAGE_SIZE_1024x1792:
	// 		imageWidth = 1024;
	// 		imageHeight = 1792;
	// 		break;
	// 	default:
	// 		printf("Invalid image size\n");
	// 		imageWidth = 256;
	// 		imageHeight = 256;
	// 		break;
	// }

	// updateStatusBar("Generating image name...", 7);
	// struct MinList *imageNameConversation = newConversation();
	// addTextToConversation(imageNameConversation, text, "user");
	// addTextToConversation(imageNameConversation, "generate a short title for this image and don't enclose the title in quotes or prefix the response with anything", "user");
	// struct json_object **responses = postChatMessageToOpenAI(imageNameConversation, config.chatModel, config.openAiApiKey, FALSE);
	
	// struct GeneratedImage *generatedImage = AllocVec(sizeof(struct GeneratedImage), MEMF_ANY);
	// if (responses == NULL) {
	// 	displayError("Failed to generate image name. Using ID instead.");
	// 	updateStatusBar("Error", 6);
	// } else if (responses[0] != NULL) {
	// 	STRPTR responseString = getMessageContentFromJson(responses[0], FALSE);
	// 	formatText(responseString);
	// 	generatedImage->name = AllocVec(strlen(responseString) + 1, MEMF_ANY | MEMF_CLEAR);
	// 	strncpy(generatedImage->name, responseString, strlen(responseString));
	// 	updateStatusBar("Ready", 5);
	// 	json_object_put(responses[0]);
	// 	FreeVec(responses);
	// } else {
	// 	generatedImage->name = AllocVec(11, MEMF_ANY | MEMF_CLEAR);
	// 	strncpy(generatedImage->name, id, 10);
	// 	updateStatusBar("Ready", 5);
	// 	if (responses != NULL) {
	// 		FreeVec(responses);
	// 	}
	// 	displayError("Failed to generate image name. Using ID instead.");
	// }
	// freeConversation(imageNameConversation);

	// generatedImage->filePath = AllocVec(strlen(fullPath) + 1, MEMF_ANY | MEMF_CLEAR);
	// strncpy(generatedImage->filePath, fullPath, strlen(fullPath));
	// generatedImage->prompt = AllocVec(strlen(text) + 1, MEMF_ANY | MEMF_CLEAR);
	// strncpy(generatedImage->prompt, text, strlen(text));
	// generatedImage->imageModel = config.imageModel;
	// generatedImage->width = imageWidth;
	// generatedImage->height = imageHeight;
	// addImageToImageList(generatedImage);
	// currentImage = generatedImage;

	// SetGadgetAttrs(createImageButton, mainWindow, NULL, GA_Disabled, TRUE, TAG_DONE);
	// SetGadgetAttrs(textInputTextEditor, mainWindow, NULL, GA_ReadOnly, TRUE, TAG_DONE);
	// SetGadgetAttrs(openSmallImageButton, mainWindow, NULL, GA_Disabled, FALSE, TAG_DONE);
	// SetGadgetAttrs(openMediumImageButton, mainWindow, NULL, GA_Disabled, FALSE, TAG_DONE);
	// SetGadgetAttrs(openLargeImageButton, mainWindow, NULL, GA_Disabled, FALSE, TAG_DONE);
	// SetGadgetAttrs(openOriginalImageButton, mainWindow, NULL, GA_Disabled, FALSE, TAG_DONE);
	// SetGadgetAttrs(saveCopyButton, mainWindow, NULL, GA_Disabled, FALSE, TAG_DONE);
	// SetGadgetAttrs(newImageButton, mainWindow, NULL, GA_Disabled, FALSE, TAG_DONE);
	// SetGadgetAttrs(deleteImageButton, mainWindow, NULL, GA_Disabled, FALSE, TAG_DONE);
	// SetGadgetAttrs(textInputTextEditor, mainWindow, NULL, GA_Disabled, FALSE, TAG_DONE);
	// SetGadgetAttrs(textInputTextEditor, mainWindow, NULL, GA_ReadOnly, TRUE, TAG_DONE);

	// FreeVec(text);
	// FreeVec(textUTF_8);
	return;
}

/**
 * Opens and displays the image with sfaling
 * @param image the image to open
 * @param width the width of the image
 * @param height the height of the image
**/ 
static void openImage(struct GeneratedImage *image, WORD scaledWidth, WORD scaledHeight) {
	if (image == NULL) return;
	WORD lowestWidth = (screen->Width - 16) < scaledWidth ? (screen->Width - 16) : scaledWidth;
	WORD lowestHeight = screen->Height < scaledHeight ? screen->Height : scaledHeight;

	// struct Object *textLabel = NewObject(STRING_GetClass(), NULL,
	// 	GA_ReadOnly, TRUE,
	// 	STRINGA_Justification, GACT_STRINGCENTER,
	// 	TAG_DONE);
	
	// struct Object *imageWindowLayout = NewObject(LAYOUT_GetClass(), NULL,
	// LAYOUT_Orientation, LAYOUT_ORIENT_VERT,
	// 	LAYOUT_AddChild, textLabel,
	// 	CHILD_MinWidth, lowestWidth,
	// 	CHILD_MinHeight, lowestHeight,
	// 	TAG_DONE);

	// if ((imageWindowObject = NewObject(WINDOW_GetClass(), NULL,
	// 	WINDOW_Position, WPOS_CENTERSCREEN,
	// 	WA_Activate, TRUE,
	// 	WA_Title, image->name,
	// 	WA_Width, lowestWidth,
	// 	WA_Height, lowestHeight,
	// 	WA_CloseGadget, TRUE,
	// 	WA_DragBar, TRUE,
	// 	WA_SizeGadget, TRUE,
	// 	WA_DepthGadget, FALSE,
	// 	WINDOW_Layout, imageWindowLayout,
	// 	WINDOW_Position, WPOS_CENTERSCREEN,
	// 	WINDOW_IDCMPHook, &idcmpHookCreateImageWindow,
	// 	WINDOW_InterpretIDCMPHook, TRUE,
	// 	WINDOW_IDCMPHookBits, IDCMP_IDCMPUPDATE | IDCMP_REFRESHWINDOW,
	// 	WA_IDCMP,			IDCMP_CLOSEWINDOW | IDCMP_NEWSIZE | IDCMP_REFRESHWINDOW | IDCMP_IDCMPUPDATE |
	// 						IDCMP_GADGETUP | IDCMP_GADGETDOWN | IDCMP_MOUSEBUTTONS |
	// 						IDCMP_MOUSEMOVE | IDCMP_VANILLAKEY |
	// 						IDCMP_RAWKEY,
	// 	WA_CustomScreen, screen,
	// 	TAG_DONE)) == NULL) {
	// 		printf("Could not create imageWindowObject\n");
	// 		return RETURN_ERROR;
	// }

	// if ((imageWindow = (struct Window *)DoMethod(imageWindowObject, WM_OPEN, NULL)) == NULL) {
	// 	printf("Could not open imageWindow\n");
	// 	return RETURN_ERROR;
	// }

	// SetGadgetAttrs(textLabel, imageWindow, NULL, STRINGA_TextVal, "Loading image...", TAG_DONE);

	// updateStatusBar("Loading image...", 7);
	// SetWindowPointer(imageWindow,
	// 							WA_BusyPointer,	TRUE,
	// 						TAG_DONE);

	// if ((dataTypeObject = NewDTObject(image->filePath,
	// 	DTA_SourceType, DTST_FILE,
	// 	DTA_GroupID, GID_PICTURE,
	// 	PDTA_Remap, TRUE,
	// 	GA_Text, "Loading image...",
	// 	GA_Left, imageWindow->BorderLeft,
	// 	GA_Top, imageWindow->BorderTop,
	// 	GA_RelWidth, imageWindow->Width - imageWindow->BorderLeft - imageWindow->BorderRight,
	// 	GA_RelHeight, imageWindow->Height - imageWindow->BorderTop - imageWindow->BorderBottom,
	// 	ICA_TARGET,	ICTARGET_IDCMP,
	// 	TAG_DONE)) == NULL) {
	// 		printf("Could not create dataTypeObject\n");
	// 		return RETURN_ERROR;	
	// }

	// updateStatusBar("Scaling image...", 8);
	// DoMethod(dataTypeObject, PDTM_SCALE, lowestWidth, lowestHeight, 0);

	// AddDTObject(imageWindow, NULL, dataTypeObject, -1);
	// RefreshDTObjects(dataTypeObject, imageWindow, NULL, NULL);

	// BOOL done = FALSE;
	// ULONG signalMask, winSignal, signals, result;
	// WORD code;

	// GetAttr(WINDOW_SigMask, imageWindowObject, &winSignal);
	// signalMask = winSignal;
	// while (!done) {
	// 	signals = Wait(signalMask);
	// 	while ((result = DoMethod(imageWindowObject, WM_HANDLEINPUT, &code)) != WMHI_LASTMSG) {
	// 		switch (result & WMHI_CLASSMASK) {
	// 			case WMHI_RAWKEY:
	// 			case WMHI_CLOSEWINDOW:
	// 			case WMHI_MOUSEBUTTONS:
	// 				done = TRUE;
	// 				break;
	// 		}
	// 	}
	// }

	// DoMethod(imageWindowObject, WM_CLOSE);
	// DisposeObject(imageWindowObject);
	// DisposeDTObject(dataTypeObject);

	// updateStatusBar("Ready", 5);
}

/**
 * Converts a UTF-8 string to ISO-8859-1 which is what the Amiga uses by default
 * Taken from https://stackoverflow.com/questions/23689733/convert-string-from-utf-8-to-iso-8859-1
 * @param utf8String the UTF-8 string to convert to ISO-8859-1
 * @return pointer to the converted string. Free it with FreeVec() when you're done with it
**/
static STRPTR UTF8ToISO8859_1(CONST_STRPTR utf8String) {
	if (utf8String == NULL)
		return NULL;

	STRPTR convertedString = AllocVec(strlen(utf8String) + 1, MEMF_ANY);
	UBYTE* convertedStringPointer = convertedString;

	UBYTE codepoint = NULL;
	while (*utf8String != 0) {
		UBYTE ch = (UBYTE)*utf8String;
		if (ch <= 0x7f)
			codepoint = ch;
		else if (ch <= 0xbf)
			codepoint = (codepoint << 6) | (ch & 0x3f);
		else if (ch <= 0xdf)
			codepoint = ch & 0x1f;
		else if (ch <= 0xef)
			codepoint = ch & 0x0f;
		else
			codepoint = ch & 0x07;
		++utf8String;
		if ((*utf8String & 0xc0) != 0x80) {
			 *convertedStringPointer = codepoint;
			convertedStringPointer++;
		}
	}
	*convertedStringPointer = '\0';

	return convertedString;
}

/**
 * Converts a UTF-8 string to ISO-8859-1 which is what the Amiga uses by default
 * Adapted from https://stackoverflow.com/questions/23689733/convert-string-from-utf-8-to-iso-8859-1
 * @param iso8859_1String the ISO-8859-1 string to convert to UTF-8
 * @return pointer to the converted string. Free it with FreeVec() when you're done with it
**/
static STRPTR ISO8859_1ToUTF8(CONST_STRPTR iso8859_1String) {
	if (iso8859_1String == NULL)
		return NULL;

	// Assume max UTF-8 size (4 bytes per character), plus null terminator
	STRPTR convertedString = AllocVec((4 * strlen(iso8859_1String) + 1) * sizeof(char), MEMF_ANY);
	UBYTE* convertedStringPointer = convertedString;

	while (*iso8859_1String != 0) {
		UBYTE ch = (UBYTE)*iso8859_1String;

		if(ch < 128) { 
			*convertedStringPointer++ = ch;
		} 
		else {
			*convertedStringPointer++ = 0xc0 | (ch >> 6);    // first byte
			*convertedStringPointer++ = 0x80 | (ch & 0x3f);  // second byte
		}
		
		++iso8859_1String;
	}
	
	*convertedStringPointer = '\0';

	return convertedString;
}

/**
 * Load the conversations from disk
 * @return RETURN_OK on success, RETURN_ERROR on failure
**/
static LONG loadConversations() {
	BPTR file = Open(PROGDIR"chat-history.json", MODE_OLDFILE);
	if (file == 0) {
		return RETURN_OK;
	}

	#ifdef __AMIGAOS3__
	Seek(file, 0, OFFSET_END);
	LONG fileSize = Seek(file, 0, OFFSET_BEGINNING);
	#else
	#ifdef __AMIGAOS4__
	int64_t fileSize = GetFileSize(file);
	#else
	struct FileInfoBlock fib;
	ExamineFH64(file, &fib, NULL);
	int64_t fileSize = fib.fib_Size;
	#endif
	#endif
	STRPTR conversationsJsonString = AllocVec(fileSize + 1, MEMF_CLEAR);
	if (Read(file, conversationsJsonString, fileSize) != fileSize) {
		displayDiskError("Failed to read from message history file. Conversation history will not be loaded", IoErr());
		Close(file);
		FreeVec(conversationsJsonString);
		return RETURN_ERROR;
	}

	Close(file);

	struct json_object *conversationsJsonArray = json_tokener_parse(conversationsJsonString);
	if (conversationsJsonArray == NULL) {
		if (Rename(PROGDIR"chat-history.json", PROGDIR"chat-history.json.bak")) {
			displayDiskError("Failed to parse chat history. Malformed JSON. The chat-history.json file is probably corrupted. Conversation history will not be loaded. A backup of the chat-history.json file has been created as chat-history.json.bak", IoErr());
		} else if (copyFile(PROGDIR"chat-history.json", "RAM:chat-history.json")) {
			displayError("Failed to parse chat history. Malformed JSON. The chat-history.json file is probably corrupted. Conversation history will not be loaded. There was an error writing a backup of the chat history to disk but a copy has been saved to RAM:chat-history.json.bak");
			#ifdef __AMIGAOS3__
			if (!DeleteFile(PROGDIR"chat-history.json")) {
			#else
			if (!Delete(PROGDIR"chat-history.json")) {
			#endif
				displayDiskError("Failed to delete chat-history.json. Please delete this file manually.", IoErr());
			}
		}

		FreeVec(conversationsJsonString);
		return RETURN_ERROR;
	}

	for (UWORD i = 0; i < json_object_array_length(conversationsJsonArray); i++) {
		struct json_object *conversationJsonObject = json_object_array_get_idx(conversationsJsonArray, i);
		struct json_object *conversationNameJsonObject;
		if (!json_object_object_get_ex(conversationJsonObject, "name", &conversationNameJsonObject)) {
			displayError("Failed to parse chat history. \"name\" is missing from the conversation. The chat-history.json file is probably corrupted. Conversation history will not be loaded.");
			FreeVec(conversationsJsonString);
			json_object_put(conversationsJsonArray);
			return RETURN_ERROR;
		}

		STRPTR conversationName = json_object_get_string(conversationNameJsonObject);

		struct json_object *messagesJsonArray;
		if (!json_object_object_get_ex(conversationJsonObject, "messages", &messagesJsonArray)) {
			displayError("Failed to parse chat history. \"messages\" is missing from the conversation. The chat-history.json file is probably corrupted. Conversation history will not be loaded.");
			FreeVec(conversationsJsonString);
			json_object_put(conversationsJsonArray);
			return RETURN_ERROR;
		}

		struct Conversation *conversation = newConversation();
		conversation->name = AllocVec(strlen(conversationName) + 1, MEMF_ANY | MEMF_CLEAR);
		strncpy(conversation->name, conversationName, strlen(conversationName));

		set(conversationListObject, MUIA_NList_Quiet, TRUE);

		for (UWORD j = 0; j < json_object_array_length(messagesJsonArray); j++) {
			struct json_object *messageJsonObject = json_object_array_get_idx(messagesJsonArray, j);
			struct json_object *roleJsonObject;
			if (!json_object_object_get_ex(messageJsonObject, "role", &roleJsonObject)) {
				displayError("Failed to parse chat history. \"role\" is missing from the conversation message. The chat-history.json file is probably corrupted. Conversation history will not be loaded.");
				FreeVec(conversationsJsonString);
				json_object_put(conversationsJsonArray);
				return RETURN_ERROR;
			}
			STRPTR role = json_object_get_string(roleJsonObject);

			struct json_object *contentJsonObject = json_object_array_get_idx(messagesJsonArray, j);
			if (!json_object_object_get_ex(messageJsonObject, "content", &contentJsonObject)) {
				displayError("Failed to parse chat history. \"content\" is missing from the conversation. The chat-history.json file is probably corrupted. Conversation history will not be loaded.");
				FreeVec(conversationsJsonString);
				json_object_put(conversationsJsonArray);
				return RETURN_ERROR;
			}
			STRPTR content = json_object_get_string(contentJsonObject);
			addTextToConversation(conversation, content, role);
		}
		DoMethod(conversationListObject, MUIM_NList_InsertSingle, conversation, MUIV_NList_Insert_Top);
		freeConversation(conversation);
	}
	set(conversationListObject, MUIA_NList_Quiet, FALSE);
	json_object_put(conversationsJsonArray);
	return RETURN_OK;
}

/**
 * Load the images from disk
 * @return RETURN_OK on success, RETURN_ERROR on failure
**/
static LONG loadImages() {
	BPTR file = Open(PROGDIR"image-history.json", MODE_OLDFILE);
	if (file == 0) {
		return RETURN_OK;
	}

	#ifdef __AMIGAOS3__
	Seek(file, 0, OFFSET_END);
	LONG fileSize = Seek(file, 0, OFFSET_BEGINNING);
	#else
	#ifdef __AMIGAOS4__
	int64_t fileSize = GetFileSize(file);
	#else
	struct FileInfoBlock fib;
	ExamineFH64(file, &fib, NULL);
	int64_t fileSize = fib.fib_Size;
	#endif
	#endif
	STRPTR imagesJsonString = AllocVec(fileSize + 1, MEMF_CLEAR);
	if (Read(file, imagesJsonString, fileSize) != fileSize) {
		displayDiskError("Failed to read from image history file. Image generation history will not be loaded", IoErr());
		Close(file);
		FreeVec(imagesJsonString);
		return RETURN_ERROR;
	}

	Close(file);

	struct json_object *imagesJsonArray = json_tokener_parse(imagesJsonString);
	if (imagesJsonArray == NULL) {
		if (Rename(PROGDIR"image-history.json", PROGDIR"image-history.json.bak")) {
			displayDiskError("Failed to parse image history. Malformed JSON. The image-history.json file is probably corrupted. Image history will not be loaded. A backup of the image-history.json file has been created as image-history.json.bak", IoErr());
		} else if (copyFile(PROGDIR"image-history.json", "RAM:image-history.json")) {
			displayError("Failed to parse image history. Malformed JSON. The image-history.json file is probably corrupted. Image history will not be loaded. There was an error writing a backup of the image history to disk but a copy has been saved to RAM:image-history.json.bak");
			#ifdef __AMIGAOS3__
			if (!DeleteFile(PROGDIR"image-history.json")) {
			#else
			if (!Delete(PROGDIR"image-history.json")) {
			#endif
				displayDiskError("Failed to delete image-history.json. Please delete this file manually.", IoErr());
			}
		}

		FreeVec(imagesJsonString);
		return RETURN_ERROR;
	}

	for (UWORD i = 0; i < json_object_array_length(imagesJsonArray); i++) {
		struct json_object *imageJsonObject = json_object_array_get_idx(imagesJsonArray, i);
		struct json_object *imageNameJsonObject;
		if (!json_object_object_get_ex(imageJsonObject, "name", &imageNameJsonObject)) {
			displayError("Failed to parse image history. \"name\" is missing from the image. The image-history.json file is probably corrupted. Image history will not be loaded.");
			FreeVec(imagesJsonString);
			json_object_put(imagesJsonArray);
			return RETURN_ERROR;
		}

		STRPTR imageName = json_object_get_string(imageNameJsonObject);

		struct json_object *imageFilePathJsonObject;
		if (!json_object_object_get_ex(imageJsonObject, "filePath", &imageFilePathJsonObject)) {
			displayError("Failed to parse image history. \"filePath\" is missing from the image. The image-history.json file is probably corrupted. Image history will not be loaded.");
			FreeVec(imagesJsonString);
			json_object_put(imagesJsonArray);
			return RETURN_ERROR;
		}

		STRPTR imageFilePath = json_object_get_string(imageFilePathJsonObject);

		struct json_object *imagePromptJsonObject;
		if (!json_object_object_get_ex(imageJsonObject, "prompt", &imagePromptJsonObject)) {
			displayError("Failed to parse image history. \"prompt\" is missing from the image. The image-history.json file is probably corrupted. Image history will not be loaded.");
			FreeVec(imagesJsonString);
			json_object_put(imagesJsonArray);
			return RETURN_ERROR;
		}

		STRPTR imagePrompt = json_object_get_string(imagePromptJsonObject);

		struct json_object *imageModelJsonObject;
		if (!json_object_object_get_ex(imageJsonObject, "imageModel", &imageModelJsonObject)) {
			displayError("Failed to parse image history. \"imageModel\" is missing from the image. The image-history.json file is probably corrupted. Image history will not be loaded.");
			FreeVec(imagesJsonString);
			json_object_put(imagesJsonArray);
			return RETURN_ERROR;
		}

		enum ImageModel imageModel = json_object_get_int(imageModelJsonObject);

		struct json_object *imageWidthJsonObject;
		if (!json_object_object_get_ex(imageJsonObject, "width", &imageWidthJsonObject)) {
			displayError("Failed to parse image history. \"width\" is missing from the image. The image-history.json file is probably corrupted. Image history will not be loaded.");
			FreeVec(imagesJsonString);
			json_object_put(imagesJsonArray);
			return RETURN_ERROR;
		}

		UWORD imageWidth = (WORD)json_object_get_int(imageWidthJsonObject);

		struct json_object *imageHeightJsonObject;
		if (!json_object_object_get_ex(imageJsonObject, "height", &imageHeightJsonObject)) {
			displayError("Failed to parse image history. \"height\" is missing from the image. The image-history.json file is probably corrupted. Image history will not be loaded.");
			FreeVec(imagesJsonString);
			json_object_put(imagesJsonArray);
			return RETURN_ERROR;
		}

		UWORD imageHeight = (WORD)json_object_get_int(imageHeightJsonObject);

		struct GeneratedImage *generatedImage = AllocVec(sizeof(struct GeneratedImage), MEMF_ANY);
		generatedImage->name = AllocVec(strlen(imageName) + 1, MEMF_ANY);
		strcpy(generatedImage->name, imageName);
		generatedImage->filePath = AllocVec(strlen(imageFilePath) + 1, MEMF_ANY);
		strcpy(generatedImage->filePath, imageFilePath);
		generatedImage->prompt = AllocVec(strlen(imagePrompt) + 1, MEMF_ANY);
		strcpy(generatedImage->prompt, imagePrompt);
		generatedImage->imageModel = imageModel;
		generatedImage->width = imageWidth;
		generatedImage->height = imageHeight;
		addImageToImageList(generatedImage);
	}

	json_object_put(imagesJsonArray);
	return RETURN_OK;
}

/**
 * Copies a file from one location to another
 * @param source The source file to copy
 * @param destination The destination to copy the file to
 * @return TRUE if the file was copied successfully, FALSE otherwise
**/
static BOOL copyFile(STRPTR source, STRPTR destination) {
	const UBYTE ERROR_MESSAGE_BUFFER_SIZE = 255;
	const UWORD FILE_BUFFER_SIZE = 4096;
	BPTR srcFile, dstFile;
	LONG bytesRead, bytesWritten;
	APTR buffer = AllocVec(FILE_BUFFER_SIZE, MEMF_ANY);
	STRPTR errorMessage = AllocVec(ERROR_MESSAGE_BUFFER_SIZE, MEMF_ANY);

	if (!(srcFile = Open(source, MODE_OLDFILE))) {
		snprintf(errorMessage, ERROR_MESSAGE_BUFFER_SIZE, "Error opening %s for copy", source);
		displayDiskError(errorMessage, IoErr());
		FreeVec(buffer);
		FreeVec(errorMessage);
		return FALSE;
	}

	if (!(dstFile = Open(destination, MODE_NEWFILE))) {
		snprintf(errorMessage, ERROR_MESSAGE_BUFFER_SIZE, "Error creating %s for copy", destination);
		displayDiskError(errorMessage, IoErr());
		FreeVec(buffer);
		FreeVec(errorMessage);
		Close(srcFile);
		return FALSE;
	}

	updateStatusBar("Copying file...", 7);

	do {
		bytesRead = Read(srcFile, buffer, FILE_BUFFER_SIZE);

		if (bytesRead > 0) {
			bytesWritten = Write(dstFile, buffer, bytesRead);

			if (bytesWritten != bytesRead) {
				updateStatusBar("Ready", 5);
				snprintf(errorMessage, ERROR_MESSAGE_BUFFER_SIZE, "Error copying %s to %s", source, destination);
				displayDiskError(errorMessage, IoErr());
				FreeVec(buffer);
				FreeVec(errorMessage);
				Close(srcFile);
				Close(dstFile);
				return FALSE;
			}
		}
		else if (bytesRead < 0) {
			updateStatusBar("Ready", 5);
			snprintf(errorMessage, ERROR_MESSAGE_BUFFER_SIZE, "Error copying %s to %s", source, destination);
			displayDiskError(errorMessage, IoErr());
			FreeVec(buffer);
			FreeVec(errorMessage);
			Close(srcFile);
			Close(dstFile);
			return FALSE;
		}
	} while (bytesRead > 0);

	updateStatusBar("Ready", 5);
	FreeVec(buffer);
	FreeVec(errorMessage);
	Close(srcFile);
	Close(dstFile);
	return TRUE;
}

/**
 * Shutdown the GUI
**/
void shutdownGUI() {
	MUI_DisposeObject(app);
	// freeImageList();
	if (isPublicScreen) {
		ReleasePen(screen->ViewPort.ColorMap, sendMessageButtonPen);
		ReleasePen(screen->ViewPort.ColorMap, newChatButtonPen);
		ReleasePen(screen->ViewPort.ColorMap, deleteButtonPen);
		UnlockPubScreen(NULL, screen);
	} else {
		CloseScreen(screen);
	}
	if (uiTextFont)
		CloseFont(uiTextFont);
	if (appPort)
		DeleteMsgPort(appPort);

	closeGUILibraries();
}