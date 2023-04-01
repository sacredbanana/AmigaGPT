#include "stringutil.h"
#include "support/gcc8_c_support.h"

#define BUFFER_SIZE 255
static UBYTE textBuffer[BUFFER_SIZE] = {0};

/**
 * C++ version 0.4 char* style "itoa":
 * Written by Lukás Chmela
 * Released under GPLv3.
 */
UBYTE* itoa(ULONG value, UBYTE* result, UBYTE base) {
    // check that the base if valid
    if (base < 2 || base > 36) { *result = '\0'; return result; }

    UBYTE* ptr = result, *ptr1 = result, tmp_char;
    int tmp_value;

    do {
        tmp_value = value;
        value /= base;
        *ptr++ = "zyxwvutsrqponmlkjihgfedcba9876543210123456789abcdefghijklmnopqrstuvwxyz" [35 + (tmp_value - value * base)];
    } while ( value );

    // Apply negative sign
    if (tmp_value < 0) *ptr++ = '-';
    *ptr-- = '\0';
    while(ptr1 < ptr) {
        tmp_char = *ptr;
        *ptr--= *ptr1;
        *ptr1++ = tmp_char;
    }
    return result;
}

void strcat(UBYTE *destination, UBYTE *source) {
    UBYTE destIndex = 0;
    while (destination[destIndex] != '\0') destIndex ++;
    UBYTE sourceIndex = 0;
    while (source[sourceIndex] != '\0') destination[destIndex++] = source[sourceIndex++]; 
}

UBYTE* buildPath(UBYTE *rootDir, UBYTE *relativePath) {
    memset(textBuffer, 0, BUFFER_SIZE);
	memcpy(textBuffer, rootDir, strlen((const char *)rootDir));
	strcat(textBuffer,relativePath);
    return textBuffer;
}