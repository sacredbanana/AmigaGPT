# to generate assembler listing with LTO, add to LDFLAGS: -Wa,-adhln=$@.listing,--listing-rhs-width=200
# for better annotations add -dA -dP
# to generate assembler source with LTO, add to LDFLAGS: -save-temps=cwd

OS := $(shell uname)

subdirs := $(wildcard */) $(wildcard src/*/) $(wildcard src/*/*/)
VPATH = $(subdirs)
BUILD_DIR := build/os3/obj/
cpp_sources := $(wildcard *.cpp) $(wildcard $(addsuffix *.cpp,$(subdirs)))
cpp_objects := $(addprefix $(BUILD_DIR),$(patsubst %.cpp,%.o,$(notdir $(cpp_sources))))
c_sources := $(wildcard *.c) $(wildcard $(addsuffix *.c,$(subdirs)))
c_objects := $(addprefix $(BUILD_DIR),$(patsubst %.c,%.o,$(notdir $(c_sources))))
s_sources := $(wildcard *.s) $(wildcard $(addsuffix *.s,$(subdirs)))
s_objects := $(addprefix $(BUILD_DIR),$(patsubst %.s,%.o,$(notdir $(s_sources))))
vasm_sources := $(wildcard *.asm) $(wildcard $(addsuffix *.asm, $(subdirs)))
vasm_objects := $(addprefix $(BUILD_DIR), $(patsubst %.asm,%.o,$(notdir $(vasm_sources))))
objects := $(cpp_objects) $(c_objects) $(s_objects) $(vasm_objects)

AUTOGEN_FILE := src/version.h
AUTOGEN_NEXT := $(shell expr $$(awk '/#define BUILD_NUMBER/' $(AUTOGEN_FILE) | tr -cd "[0-9]") + 1)

# https://stackoverflow.com/questions/4036191/sources-from-subdirectories-in-makefile/4038459
# http://www.microhowto.info/howto/automatically_generate_makefile_dependencies.html

PROGRAM_NAME = AmigaGPT
EXECUTABLE_OUT = out/$(PROGRAM_NAME)
CC = m68k-amigaos-gcc
VASM = vasmm68k_mot

LIBDIR = /opt/amiga/m68k-amigaos/lib
SDKDIR = /opt/amiga/m68k-amigaos/sys-include
NDKDIR = /opt/amiga/m68k-amigaos/ndk-include
INCDIR = /opt/amiga/m68k-amigaos/include

ifeq ($(OS),Darwin)
	SED = sed -i "" 
else
	SED = sed -i
endif

CCFLAGS = -g -MP -MMD -m68020 -Ofast -Wextra -Wno-unused-function -Wno-discarded-qualifiers -Wno-int-conversion -Wno-volatile-register-var -fomit-frame-pointer -fno-tree-loop-distribution -flto -fwhole-program -fno-exceptions -noixemul -fbaserel -L$(LIBDIR) -lamiga -lm -lamisslstubs -D__AMIGAOS3__ -DPROGRAM_NAME=\"$(PROGRAM_NAME)\"
CPPFLAGS= $(CCFLAGS) -fno-rtti -fcoroutines -fno-use-cxa-atexit
ASFLAGS = -Wa,-g,--register-prefix-optional,-I$(SDKDIR),-I$(NDKDIR),-I$(INCDIR),-D
LDFLAGS =  -Wl,-Map=$(EXECUTABLE_OUT).map
VASMFLAGS = -m68020 -Fhunk -opt-fconst -nowarn=62 -dwarf=3 -quiet -x -I. -D__AMIGAOS3__ -DPROGRAM_NAME=\"$(PROGRAM_NAME)\" -I$(INCDIR) -I$(SDKDIR) -I$(NDKDIR)

.PHONY: all clean copy_bundle_files

all: copy_bundle_files $(EXECUTABLE_OUT)

$(BUILD_DIR):
	@$(info Creating directory $@)
	@mkdir -p $@

$(EXECUTABLE_OUT): $(objects)
	$(info Linking $(PROGRAM_NAME))
	$(CC) $(CCFLAGS) $(LDFLAGS) $(objects) -o $@

clean:
	$(info Cleaning...)
	@$(RM) -f $(EXECUTABLE_OUT)
	@$(RM) $(BUILD_DIR)*

-include $(objects:.o=.d)

$(cpp_objects) : build/os3/obj/%.o : %.cpp | build/os3/obj/%.dir
	$(info Compiling $<)
	$(CC) $(CPPFLAGS) -c -o $@ $(CURDIR)/$<

$(c_objects) : build/os3/obj/%.o : %.c | $(BUILD_DIR)
	$(info Compiling $<)
	$(SED) 's|#define BUILD_NUMBER ".*"|#define BUILD_NUMBER "$(AUTOGEN_NEXT)"|' $(AUTOGEN_FILE)
	$(CC) $(CCFLAGS) -c -o $@ $(CURDIR)/$<

$(s_objects): build/os3/obj/%.o : %.s |$(BUILD_DIR)
	$(info Assembling $<)
	$(CC) $(CCFLAGS) $(ASFLAGS) -c -o $@ $(CURDIR)/$<

$(vasm_objects): build/os3/obj/%.o : %.asm | $(BUILD_DIR)
	$(info Assembling $<)
	$(VASM) $(VASMFLAGS) -o $@ $(CURDIR)/$<

copy_bundle_files:
	$(info Copying bundle files...)
	mkdir -p out
	cp assets/AmigaGPT_OS3.info bundle/$(PROGRAM_NAME)/$(PROGRAM_NAME).info
	cp -R bundle/$(PROGRAM_NAME)/* out/
