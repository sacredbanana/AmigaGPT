#pragma once

#include <proto/exec.h>

#ifdef __cplusplus
	extern "C" {
#endif

BYTE minByte(BYTE, BYTE);
UBYTE minUByte(UBYTE, UBYTE);
WORD minWord(WORD, WORD);
UWORD minUWord(UWORD, UWORD);
LONG minLong(LONG, LONG);
ULONG minULong(ULONG, ULONG);
BYTE maxByte(BYTE, BYTE);
UBYTE maxUByte(UBYTE, UBYTE);
WORD maxWord(WORD, WORD);
UWORD maxUWord(UWORD, UWORD);
LONG maxLong(LONG, LONG);
ULONG maxULong(ULONG, ULONG);

#ifdef __cplusplus
	} 
#endif