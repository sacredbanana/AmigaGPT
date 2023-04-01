#pragma once

#include <proto/exec.h>
#include <devices/console.h>

extern struct IOStdReq *ConsoleReadIORequest;
extern struct MsgPort *ConsoleReadPort;
extern struct IOStdReq *ConsoleWriteIORequest;
extern struct MsgPort *ConsoleWritePort;

BOOL getCursorEnabled();
void setCursorEnabled(BOOL isEnabled);
BOOL getScrollingEnabled();
void setScrollingEnabled(BOOL isEnabled);
BOOL getAutowrapEnabled();
void setAutowrapEnabled(BOOL isEnabled);
ULONG getPageLength();
void setPageLength(ULONG length);
ULONG getLineLength();
void setLineLength(ULONG length);
ULONG getLeftOffset();
void setLeftOffset(ULONG offset);
ULONG getTopOffset();
void setTopOffset(ULONG offset);
void consolePrintText(UBYTE* text);
void consolePrintTextWithLength(UBYTE* text, ULONG length);
void insertSpaces(ULONG amount);
void moveCursorUp(ULONG amount);
void moveCursorDown(ULONG amount);
void moveCursorLeft(ULONG amount);
void moveCursorRight(ULONG amount);
void moveCursorNextLine(ULONG amount);
void moveCursorPreviousLine(ULONG amount);
void moveCursorToPosition(ULONG row, ULONG column);
void moveCursorToNextHorizontalTab(ULONG amount);
void moveCursorToPreviousHorizontalTab(ULONG amount);
void eraseToEndOfDisplay();
void eraseToEndOfLine();
void insertLine();
void deleteLine();
void deleteCharacter(ULONG amount);
void scrollUp(ULONG amount);
void scrollDown(ULONG amount);
void setTab();
void clearTab();
void clearAllTabs();
void setLinefeedMode();
void setNewlineMode();
void printDeviceStatusReport();
void printWindowStatusReport();
void selectGraphicRendition(UBYTE style, UBYTE characterColor, UBYTE characterCellColor);
