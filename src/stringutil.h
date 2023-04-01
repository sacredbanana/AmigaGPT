#pragma once

#include <proto/exec.h>

#ifdef __cplusplus
	extern "C" {
#endif

UBYTE* itoa(ULONG value, UBYTE *result, UBYTE base);
void strcat(UBYTE *destination, UBYTE *source);
UBYTE* buildPath(UBYTE *rootDir, UBYTE *relativePath);

#ifdef __cplusplus
	} 
#endif