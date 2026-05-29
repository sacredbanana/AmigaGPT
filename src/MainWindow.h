#include <proto/dos.h>
#include <intuition/intuition.h>

struct Conversation;

extern struct Window *mainWindow;
extern Object *mainWindowObject;
extern Object *newChatButton;
extern Object *deleteChatButton;
extern Object *sendMessageButton;
extern Object *chatInputTextEditor;
extern Object *chatOutputTextEditor;
extern Object *chatOutputScroller;
extern Object *statusBar;
extern Object *conversationListObject;
extern Object *loadingBar;
extern Object *imageListObject;
extern Object *imageInputTextEditor;
extern Object *createImageButton;
extern Object *newImageButton;
extern Object *deleteImageButton;
extern Object *imageView;
extern Object *modeRegisterGroup;
extern STRPTR chatOutputTextEditorContents;
extern WORD pens[NUMDRIPENS + 1];
extern LONG sendMessageButtonPen;
extern LONG newChatButtonPen;
extern LONG deleteButtonPen;

/**
 * Create the main window
 * @return RETURN_OK on success, RETURN_ERROR on failure
 **/
LONG createMainWindow();

/** Phase 7c: remove chat wheel EH before MUI_DisposeObject(app). */
void chatOutputWheelShutdown(void);

/** Phase 7c: delete chat NFloattext subclass after MUI_DisposeObject(app). */
void chatOutputWheelDisposeClass(void);

/** Current chat (NULL if none). For tools like the code-block viewer. */
struct Conversation *getCurrentConversation(void);

/**
 * Prints the conversation to the conversation window
 * @param conversation the conversation to display
 **/
void displayConversation(struct Conversation *conversation);

#ifdef __MORPHOS__
/** Apply config.fixedWidthFonts to chat/image input editors (not chat Scintilla on MorphOS). */
void applyFixedWidthFontsSetting(void);
/** MorphOS: chat output font from config.chatFixedWidthFont / config.chatFontSize. */
void applyChatFontSetting(void);

/** Rebuild chat Scintilla from chatOutputTextEditorContents; preserveViewport for Markdown toggle. */
void chatOutputUpdateFromBuffer(BOOL preserveViewport);

/** First NewInput tick: finish chat Scintilla + mouse guard (not in createMainWindow). */
void morphosRunStartupDeferred(void);

/** PushMethod: heavy SetUtf8/styling outside list-click / displayConversation stack. */
void morphosScheduleChatOutputRefresh(BOOL preserveViewport);

/** Like above but raw UTF-8 + role bytes only (NList chat switch; avoids Markdown freeze). */
void morphosScheduleChatOutputRefreshFromList(void);
#endif

/** Before MUI_DisposeObject(app): close UI, clear lists, drop window notifies. */
void mainWindowPrepareShutdown(void);

void mainWindowSignalQuit(void);
BOOL mainWindowIsShuttingDown(void);
/** TRUE during mainWindowPrepareShutdown() (MorphOS UI teardown). */
BOOL mainWindowMorphosPrepareShutdownActive(void);

/** After MUI_DisposeObject(app): avoid stale Object pointers and pen IDs. */
void mainWindowInvalidateAfterShutdown(void);

/**
 * Print the conversation text to the printer
 * @return RETURN_OK on success, RETURN_ERROR on failure
 **/
LONG printConversation();