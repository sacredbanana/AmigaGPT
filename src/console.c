#include "console.h"
#include "math.h"
#include "support/gcc8_c_support.h"
#include "amiga.h"
#include "stringutil.h"
#include <exec/ports.h>
#include <proto/dos.h>
#include <devices/conunit.h>
#include <intuition/intuition.h>

BOOL isCursorEnabled;
BOOL isScrollingEnabled;
BOOL isAutowrapEnabled;
ULONG pageLength;
ULONG lineLength;
ULONG leftOffset;
ULONG topOffset;
void performConsoleCommandWithAmount(UBYTE command, UBYTE amount);
void performConsoleCommand(UBYTE command);
void readInputStreamSynchronous();

// Avoid problems from the console device locking all layers. See page 67 of the Amiga ROM Kernel Reference Manual Devices (3rd ed.) page 67 for details.
#define MAX_CONSOLE_CHUNK_SIZE 256
#define READ_BUFFER_SIZE 32

#define CONTROL_SEQUENCE_BELL 0x07 // Flash the display -- do an Intuition DisplayBeep()
#define CONTROL_SEQUENCE_BACKSPACE 0x08 // Move left one column
#define CONTROL_SEQUENCE_HORIZONTAL_TAB 0x09 // Move right one tab stop
#define CONTROL_SEQUENCE_LINEFEED 0x0A // Move down one text line as specified by he mode function
#define CONTROL_SEQUENCE_VERTICAL_TAB 0x0B // Move up one text line
#define CONTROL_SEQUENCE_FORMFEED 0x0C // Clear the console's window
#define CONTROL_SEQUENCE_CARRIAGE_RETURN 0x0D // Move to the first column
#define CONTROL_SEQUENCE_SHIFT_IN 0x0E // Undo SHIFT OUT
#define CONTROL_SEQUENCE_SHIFT_OUT 0x0F // Set MSB of each character before displaying
#define CONTROL_SEQUENCE_ESCAPE 0x1B // Escape, can be part of a control sequence indicator
#define CONTROL_SEQUENCE_INDEX 0x84 // Move the active position down one line
#define CONTROL_SEQUENCE_NEXT_LINE 0x85 // Go to the beginning of the next line
#define CONTROL_SEQUENCE_HORIZONTAL_TABULATION_SET 0x88 // Set a tab at the cursor position
#define CONTROL_SEQUENCE_REVERSE_INDEX 0x8D // Move the active position up one line
#define CONTROL_SEQUENCE_CSI 0x9B // Control Sequence Introducer
#define CONTROL_SEQUENCE_RESET_TO_INITIAL_STATE 0x1B0x63

void consolePrintText(UBYTE *text) {
    consolePrintTextWithLength(text, -1);
}

void consolePrintTextWithLength(UBYTE *text, ULONG length) {
	ULONG charactersRemaining = (length == -1) ? strlen((char *)text) : length;
	ULONG characterIndex = 0;

	while (charactersRemaining > 0) {
		ULONG chunk = minULong(charactersRemaining, (ULONG)MAX_CONSOLE_CHUNK_SIZE);
		ConsoleWriteIORequest->io_Data = text + characterIndex;
		ConsoleWriteIORequest->io_Length = chunk;
		ConsoleWriteIORequest->io_Command = CMD_WRITE;
		DoIO((struct IORequest *)ConsoleWriteIORequest);
		characterIndex += chunk;
		charactersRemaining -= chunk;
	}
}

void performConsoleCommandWithAmount(UBYTE command, UBYTE amount) {
	UBYTE numberString[7] = {0};
	itoa(amount, numberString, 10);
	ULONG numberStringLength = strlen((const char *)numberString);
    UBYTE fullCommand[10] = {0};
	fullCommand[0] = CONTROL_SEQUENCE_CSI;
	for (UBYTE i = 0; i < numberStringLength; i++) {
		fullCommand[1+i] = numberString[i];
	}
	fullCommand[1+numberStringLength] = command;
	consolePrintText(fullCommand);
}

void performConsoleCommand(UBYTE command) {
    UBYTE fullCommand[] = {CONTROL_SEQUENCE_CSI, command};
	consolePrintTextWithLength(fullCommand, 2);
}

void readInputStreamSynchronous() {
	UBYTE buffer[READ_BUFFER_SIZE] = {0};
	ConsoleReadIORequest->io_Command = CMD_READ;
	ConsoleReadIORequest->io_Data = buffer; 
	ConsoleReadIORequest->io_Length = READ_BUFFER_SIZE;
	DoIO((struct IORequest *)ConsoleReadIORequest);
	consolePrintTextWithLength(buffer+1, (ULONG)strlen((char *)buffer)-2);
}

BOOL getCursorEnabled() {
	return isCursorEnabled;
}

void setCursorEnabled(BOOL isEnabled) {
	isCursorEnabled = isEnabled;
	if (isEnabled) {
		UBYTE command[] = {CONTROL_SEQUENCE_ESCAPE, '[', ' ', 'p'};
    	consolePrintTextWithLength(command, 4);
	} else {
		UBYTE command[] = {CONTROL_SEQUENCE_ESCAPE, '[', '0', ' ', 'p'};
    	consolePrintTextWithLength(command, 5);
	}
}

BOOL getScrollingEnabled() {
	return isScrollingEnabled;
}

void setScrollingEnabled(BOOL isEnabled) {
	isScrollingEnabled = isEnabled;
	if (isEnabled) {
		UBYTE command[] = {CONTROL_SEQUENCE_CSI, 0x3e, 0x31, 0x68};
    	consolePrintTextWithLength(command, 4);
	} else {
		UBYTE command[] = {CONTROL_SEQUENCE_CSI, 0x3e, 0x31, 0x6c};
    	consolePrintTextWithLength(command, 4);
	}
}

BOOL getAutowrapEnabled() {
    return isAutowrapEnabled;
}

void setAutowrapEnabled(BOOL isEnabled) {
	isAutowrapEnabled = isEnabled;
	if (isEnabled) {
		UBYTE command[] = {CONTROL_SEQUENCE_CSI, 0x3e, 0x37, 0x68};
    	consolePrintTextWithLength(command, 4);
	} else {
		UBYTE command[] = {CONTROL_SEQUENCE_CSI, 0x3e, 0x37, 0x6c};
    	consolePrintTextWithLength(command, 4);
	}
}

ULONG getPageLength() {
	return pageLength;
}

void setPageLength(ULONG length) {
	pageLength = length;
	performConsoleCommandWithAmount(0x74, length);
}

ULONG getLineLength() {
	return lineLength;
}

void setLineLength(ULONG length) {
	lineLength = length;
	performConsoleCommandWithAmount(0x75, length);
}

ULONG getLeftOffset() {
	return leftOffset;
}

void setLeftOffset(ULONG offset) {
	leftOffset = offset;
	performConsoleCommandWithAmount(0x78, offset);
}

ULONG getTopOffset() {
	return topOffset;
}

void setTopOffset(ULONG offset) {
	topOffset = offset;
	performConsoleCommandWithAmount(0x79, offset);
}

void insertSpaces(ULONG amount) {
	performConsoleCommandWithAmount(0x40, amount);
}

void moveCursorUp(ULONG amount) {
	performConsoleCommandWithAmount(0x41, amount);
}

void moveCursorDown(ULONG amount) {
	performConsoleCommandWithAmount(0x42, amount);
}

void moveCursorRight(ULONG amount) {
	performConsoleCommandWithAmount(0x43, amount);
}

void moveCursorLeft(ULONG amount) {
	performConsoleCommandWithAmount(0x44, amount);
}

void moveCursorNextLine(ULONG amount) {
	performConsoleCommandWithAmount(0x45, amount);
}

void moveCursorPreviousLine(ULONG amount) {
	performConsoleCommandWithAmount(0x46, amount);
}

void moveCursorToPosition(ULONG row, ULONG column) {
	UBYTE rowString[7] = {0};
	itoa(row, rowString, 10);
	ULONG rowStringLength = strlen((const char *)rowString);
	UBYTE columnString[7] = {0};
	itoa(column, columnString, 10);
	ULONG columnStringLength = strlen((const char *)columnString);
    UBYTE fullCommand[10] = {0};
	fullCommand[0] = CONTROL_SEQUENCE_CSI;
	for (UBYTE i = 0; i < rowStringLength; i++) {
		fullCommand[1+i] = rowString[i];
	}
	fullCommand[1+rowStringLength] = 0x3b; 
	for (UBYTE i = 0; i < columnStringLength; i++) {
		fullCommand[2+i+rowStringLength] = columnString[i];
	}
	fullCommand[2+rowStringLength+columnStringLength] = 0x48;
	consolePrintText(fullCommand);
}

void moveCursorToNextHorizontalTab(ULONG amount) {
	performConsoleCommandWithAmount(0x49, amount);
}

void moveCursorToPreviousHorizontalTab(ULONG amount) {
	performConsoleCommandWithAmount(0x5a, amount);
}

void eraseToEndOfDisplay() {
	performConsoleCommand(0x4a);
}

void eraseToEndOfLine() {
	performConsoleCommand(0x4b);
}

void insertLine() {
	performConsoleCommand(0x4c);
}

void deleteLine() {
	performConsoleCommand(0x4d);
}

void CdeleteCharacter(ULONG amount) {
	performConsoleCommandWithAmount(0x50, amount);
}

void scrollUp(ULONG amount) {
	performConsoleCommandWithAmount(0x53, amount);
}

void scrollDown(ULONG amount) {
	performConsoleCommandWithAmount(0x54, amount);
}

void setTab() {
	performConsoleCommandWithAmount(0x57, 0);
}

void clearTab() {
	performConsoleCommandWithAmount(0x57, 2);
}

void clearAllTabs() {
	performConsoleCommandWithAmount(0x57, 5);
}

void setLinefeedMode() {
	UBYTE command[] = {CONTROL_SEQUENCE_CSI, 0x32, 0x30, 0x68};
    consolePrintTextWithLength(command, 4);
}

void setNewlineMode() {
	UBYTE command[] = {CONTROL_SEQUENCE_CSI, 0x32, 0x30, 0x6c};
    consolePrintTextWithLength(command, 4);
}

void printDeviceStatusReport() {
	UBYTE command[] = {CONTROL_SEQUENCE_CSI, 0x36, 0x6e};
	consolePrintTextWithLength(command, 3);
	readInputStreamSynchronous();
}

void printWindowStatusReport() {
	UBYTE command[] = {CONTROL_SEQUENCE_CSI, 0x30, 0x20, 0x71};
	consolePrintTextWithLength(command, 4);
	readInputStreamSynchronous();
}

void selectGraphicRendition(UBYTE style, UBYTE characterColor, UBYTE characterCellColor) {
	UBYTE styleString[2] = {0};
	itoa(style, styleString, 10);
	ULONG styleStringLength = strlen((const char *)styleString);
    UBYTE fullCommand[10] = {0};
	fullCommand[0] = CONTROL_SEQUENCE_CSI;
	for (UBYTE i = 0; i < styleStringLength; i++) {
		fullCommand[1+i] = styleString[i];
	}
	fullCommand[1+styleStringLength] = 0x3b;
	fullCommand[2+styleStringLength] = 0x33;
	fullCommand[3+styleStringLength] = characterColor + 0x30;
	fullCommand[4+styleStringLength] = 0x3b;
	fullCommand[5+styleStringLength] = 0x34;
	fullCommand[6+styleStringLength] = characterCellColor + 0x30;
	fullCommand[7+styleStringLength] = 0x6d;
	consolePrintText(fullCommand);
}
