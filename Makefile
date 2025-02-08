# to generate assembler listing with LTO, add to LDFLAGS: -Wa,-adhln=$@.listing,--listing-rhs-width=200
# for better annotations add -dA -dP
# to generate assembler source with LTO, add to LDFLAGS: -save-temps=cwd
OS := $(shell uname)

subdirs := $(wildcard */) $(wildcard src/*/) $(wildcard src/*/*/)
VPATH = $(subdirs)
SOURCE_DIR = src
BUILD_DIR = build/os3/obj
BUNDLE_DIR = bundle
CATALOG_DIR = $(BUNDLE_DIR)/AmigaGPT/catalogs
CATALOG_DEFINITION = $(CATALOG_DIR)/amigagpt.cd
catalog_translations := $(wildcard $(CATALOG_DIR)/*.ct)
catalogs := $(patsubst %.ct,%.catalog,$(catalog_translations))
cpp_sources := $(wildcard *.cpp) $(wildcard $(addsuffix *.cpp,$(subdirs)))
cpp_objects := $(addprefix $(BUILD_DIR)/,$(patsubst %.cpp,%.o,$(notdir $(cpp_sources))))
c_sources := $(wildcard *.c) $(wildcard $(addsuffix *.c,$(subdirs)))
c_sources := $(filter-out $(SOURCE_DIR)/test/%, $(c_sources))
c_objects := $(addprefix $(BUILD_DIR)/,$(patsubst %.c,%.o,$(notdir $(c_sources))))
s_sources := $(wildcard *.s) $(wildcard $(addsuffix *.s,$(subdirs)))
s_objects := $(addprefix $(BUILD_DIR)/,$(patsubst %.s,%.o,$(notdir $(s_sources))))
vasm_sources := $(wildcard *.asm) $(wildcard $(addsuffix *.asm, $(subdirs)))
vasm_objects := $(addprefix $(BUILD_DIR)/, $(patsubst %.asm,%.o,$(notdir $(vasm_sources))))
objects := $(cpp_objects) $(c_objects) $(s_objects) $(vasm_objects)

AUTOGEN_FILE = $(SOURCE_DIR)/version.h
AUTOGEN_NEXT = $(shell expr $$(awk '/#define BUILD_NUMBER/' $(AUTOGEN_FILE) | tr -cd "[0-9]") + 1)

GIT_BRANCH = $(shell git rev-parse --abbrev-ref HEAD)
GIT_COMMIT = $(shell git rev-parse HEAD)
GIT_TIMESTAMP = $(shell git log -1 --format=%cd --date=format:"%Y-%m-%d~%H:%M:%S")

# https://stackoverflow.com/questions/4036191/sources-from-subdirectories-in-makefile/4038459
# http://www.microhowto.info/howto/automatically_generate_makefile_dependencies.html

PROGRAM_NAME = AmigaGPT
EXECUTABLE_DIR = out
EXECUTABLE_OUT = $(EXECUTABLE_DIR)/$(PROGRAM_NAME)
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

CCFLAGS = -MP -MMD -m68020 -Wextra -Wno-unused-function -Wno-discarded-qualifiers -Wno-int-conversion -Wno-volatile-register-var -fomit-frame-pointer -fno-tree-loop-distribution -fno-exceptions -noixemul -fbaserel -lamiga -lm -lamisslstubs -lmui -D__AMIGAOS3__ -DPROGRAM_NAME=\"$(PROGRAM_NAME)\" -DGIT_BRANCH=\"$(GIT_BRANCH)\" -DGIT_COMMIT=\"$(GIT_COMMIT)\" -DGIT_TIMESTAMP=\"$(GIT_TIMESTAMP)\"
ifeq ($(DEBUG),1)
	CCFLAGS += -DPROGDIR=\"OUT:\" -DDEBUG -g -O0
else
# Writing to disk crashes inside hooks in anything higher than -O1
	CCFLAGS += -DPROGDIR=\"PROGDIR:\" -O1
endif
CPPFLAGS= $(CCFLAGS) -fno-rtti -fcoroutines -fno-use-cxa-atexit
ASFLAGS = -Wa,-g,--register-prefix-optional,-I$(SDKDIR),-I$(NDKDIR),-I$(INCDIR),-D
LDFLAGS =  -Wl,-Map=$(EXECUTABLE_OUT).map,-L$(LIBDIR),-lamiga,-lm,-lamisslstubs,-ljson-c,-lmui
VASMFLAGS = -m68020 -Fhunk -opt-fconst -nowarn=62 -dwarf=3 -quiet -x -I. -D__AMIGAOS3__  -DPROGRAM_NAME=\"$(PROGRAM_NAME)\" -I$(INCDIR) -I$(SDKDIR) -I$(NDKDIR)

.PHONY: all clean copy_bundle_files

all: $(EXECUTABLE_OUT) copy_bundle_files

clean:
	$(info Cleaning...)
	@$(RM) $(SOURCE_DIR)/amigagpt_cat.h
	@$(RM) $(SOURCE_DIR)/amigagpt_cat.c
	@$(RM) $(CATALOG_DIR)/*.catalog
	@$(RM) -f $(EXECUTABLE_OUT)
	@$(RM) $(BUILD_DIR)/*

$(SOURCE_DIR)/amigagpt_cat.h:
	@flexcat $(CATALOG_DEFINITION) $@=C_h.sd

$(SOURCE_DIR)/amigagpt_cat.c:
	@flexcat $(CATALOG_DEFINITION) $@=C_c.sd

$(BUILD_DIR):
	@$(info Creating directory $@)
	@mkdir -p $@

$(EXECUTABLE_DIR):
	@$(info Creating directory $@)
	@mkdir -p $@

$(EXECUTABLE_OUT): $(EXECUTABLE_DIR) $(SOURCE_DIR)/amigagpt_cat.h $(SOURCE_DIR)/amigagpt_cat.c $(catalogs) $(objects)
	$(info Linking $(PROGRAM_NAME))
	$(CC) $(CCFLAGS) $(LDFLAGS) $(objects) -o $@ $(LDFLAGS) 

-include $(objects:.o=.d)

$(catalogs): $(CATALOG_DIR)/%.catalog : $(CATALOG_DIR)/%.ct
	$(info Compiling catalog $<)
	flexcat $(CATALOG_DEFINITION) $(CURDIR)/$< CATALOG $@

$(cpp_objects) : $(BUILD_DIR)/%.o : %.cpp | $(BUILD_DIR)/%.dir
	$(info Compiling $<)
	$(CC) $(CPPFLAGS) -c -o $@ $(CURDIR)/$<

$(c_objects) : $(BUILD_DIR)/%.o : %.c | $(BUILD_DIR)
	$(info Compiling $<)
	$(SED) 's|#define BUILD_NUMBER ".*"|#define BUILD_NUMBER "$(AUTOGEN_NEXT)"|' $(AUTOGEN_FILE)
	$(CC) $(CCFLAGS) -c -o $@ $(CURDIR)/$<

$(s_objects): $(BUILD_DIR)/%.o : %.s | $(BUILD_DIR)
	$(info Assembling $<)
	$(CC) $(CCFLAGS) $(ASFLAGS) -c -o $@ $(CURDIR)/$<

$(vasm_objects): $(BUILD_DIR)/%.o : %.asm | $(BUILD_DIR)
	$(info Assembling $<)
	$(VASM) $(VASMFLAGS) -o $@ $(CURDIR)/$<

copy_bundle_files:
	$(info Copying bundle files...)
	cp assets/$(PROGRAM_NAME).info $(BUNDLE_DIR)/AmigaGPT/$(PROGRAM_NAME).info
	cp -R $(BUNDLE_DIR)/AmigaGPT/* $(EXECUTABLE_DIR)/
