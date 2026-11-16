# BestTextEditor.mcc and BestTextEditor.mcp - AmigaOS 3
CC = m68k-amigaos-gcc
STRIP = m68k-amigaos-strip
PLATFORM = OS3
BUILD_DIR = build/os3

include common.mk

# No -fbaserel: dispatchers and hooks would run with a foreign a4
CCFLAGS = $(COMMON_CCFLAGS) -m68020 -noixemul -fomit-frame-pointer \
	-DNO_INLINE_STDARG -D__AMIGAOS3__ -D__amigaos3__
LDFLAGS = -nostartfiles -noixemul -m68020
LDLIBS = -lamiga -lmui
STRIPFLAGS = --preserve-dates -R.comment -S
