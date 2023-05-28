# to generate assembler listing with LTO, add to LDFLAGS: -Wa,-adhln=$@.listing,--listing-rhs-width=200
# for better annotations add -dA -dP
# to generate assembler source with LTO, add to LDFLAGS: -save-temps=cwd

ifdef OS
	WINDOWS = 1
	SHELL = cmd.exe
endif

subdirs := $(wildcard */) $(wildcard src/*/) $(wildcard src/*/*/)
VPATH = $(subdirs)
cpp_sources := $(wildcard *.cpp) $(wildcard $(addsuffix *.cpp,$(subdirs)))
cpp_objects := $(addprefix obj/,$(patsubst %.cpp,%.o,$(notdir $(cpp_sources))))
c_sources := $(wildcard *.c) $(wildcard $(addsuffix *.c,$(subdirs)))
c_objects := $(addprefix obj/,$(patsubst %.c,%.o,$(notdir $(c_sources))))
s_sources := $(wildcard *.s) $(wildcard $(addsuffix *.s,$(subdirs)))
s_objects := $(addprefix obj/,$(patsubst %.s,%.o,$(notdir $(s_sources))))
vasm_sources := $(wildcard *.asm) $(wildcard $(addsuffix *.asm, $(subdirs)))
vasm_objects := $(addprefix obj/, $(patsubst %.asm,%.o,$(notdir $(vasm_sources))))
objects := $(cpp_objects) $(c_objects) $(s_objects) $(vasm_objects)

AUTOGEN_FILE := src/version.h
AUTOGEN_NEXT := $(shell expr $$(awk '/#define BUILD_NUMBER/' $(AUTOGEN_FILE) | tr -cd "[0-9]") + 1)


# https://stackoverflow.com/questions/4036191/sources-from-subdirectories-in-makefile/4038459
# http://www.microhowto.info/howto/automatically_generate_makefile_dependencies.html

program = out/amigagpt
OUT = $(program)
CC = /opt/amiga/bin/m68k-amigaos-gcc
VASM = /opt/amiga/bin/vasmm68k_mot

ifdef WINDOWS
	SDKDIR = $(abspath $(dir $(shell where $(CC)))..\m68k-amiga-elf\sys-include)
else
	# SDKDIR = $(abspath $(dir $(shell which $(CC)))../m68k-amiga-elf/sys-include)
	SDKDIR = /opt/amiga/m68k-amigaos/sys-include
	NDKDIR = /opt/amiga/m68k-amigaos/ndk-include
	INCDIR = /opt/amiga/m68k-amigaos/include
endif

CCFLAGS = -g -MP -MMD -m68000 -Ofast -Wextra -Wno-unused-function -Wno-discarded-qualifiers -Wno-int-conversion -Wno-volatile-register-var -fomit-frame-pointer -fno-tree-loop-distribution -flto -fwhole-program -fno-exceptions -D__NOLIBBASE__ -noixemul -fbaserel -lamiga -lm
CPPFLAGS= $(CCFLAGS) -fno-rtti -fcoroutines -fno-use-cxa-atexit
ASFLAGS = -Wa,-g,--register-prefix-optional,-I$(SDKDIR),-I$(NDKDIR),-I$(INCDIR),-D
LDFLAGS =  -Wl,-Map=$(OUT).map
VASMFLAGS = -m68000 -Fhunk -opt-fconst -nowarn=62 -dwarf=3 -quiet -x -I. -I$(INCDIR) -I$(SDKDIR) -I$(NDKDIR)

all: $(OUT).exe

$(OUT).exe: $(objects)
	$(info Linking $(program).exe)
	$(CC) $(CCFLAGS) $(LDFLAGS) $(objects) -o $@

clean:
	$(info Cleaning...)
ifdef WINDOWS
	@del /q obj\* out\*
else
	@$(RM) obj/* out/*
endif

-include $(objects:.o=.d)

$(cpp_objects) : obj/%.o : %.cpp
	$(info Compiling $<)
	$(CC) $(CPPFLAGS) -c -o $@ $(CURDIR)/$<

$(c_objects) : obj/%.o : %.c
	$(info Compiling $<)
	sed -i "" 's|#define BUILD_NUMBER ".*"|#define BUILD_NUMBER "$(AUTOGEN_NEXT)"|' $(AUTOGEN_FILE)
	$(CC) $(CCFLAGS) -c -o $@ $(CURDIR)/$<
	$(info Cleaning... $(AUTOGEN_NEXT))

$(s_objects): obj/%.o : %.s
	$(info Assembling $<)
	$(CC) $(CCFLAGS) $(ASFLAGS) -c -o $@ $(CURDIR)/$<

$(vasm_objects): obj/%.o : %.asm
	$(info Assembling $<)
	$(VASM) $(VASMFLAGS) -o $@ $(CURDIR)/$<
