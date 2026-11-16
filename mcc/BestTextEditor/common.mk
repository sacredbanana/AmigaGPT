# BestTextEditor.mcc and BestTextEditor.mcp - rules shared by all platforms.
# The platform Makefiles set CC, STRIP, PLATFORM, BUILD_DIR, CCFLAGS,
# LDFLAGS, LDLIBS and STRIPFLAGS, then include this file.
#
# The libraries land in OUT_DIR as BestTextEditor_<PLATFORM>.mcc/.mcp and the
# installer renames them. The preferences class catalogs are compiled next to
# their .po files in catalogs/<language>/.

OUT_DIR ?= ../../out

MCC = $(OUT_DIR)/BestTextEditor_$(PLATFORM).mcc
MCP = $(OUT_DIR)/BestTextEditor_$(PLATFORM).mcp

MCC_SOURCES = src/library.c src/BestTextEditor.c src/BestTextEditorBuffer.c \
	src/BestTextEditorDraw.c src/BestTextEditorInput.c \
	src/BestTextEditorLayout.c src/BestTextEditorText.c
MCP_SOURCES = mcp/library.c mcp/BestTextEditorPrefs.c mcp/catalog.c

MCC_OBJECTS = $(patsubst src/%.c,$(BUILD_DIR)/mcc/%.o,$(MCC_SOURCES))
MCP_OBJECTS = $(patsubst mcp/%.c,$(BUILD_DIR)/mcp/%.o,$(MCP_SOURCES))

CATALOG_DEFINITION = catalogs/BestTextEditor_mcp.pot
CATALOG_SOURCES = mcp/BestTextEditor_mcp_cat.c mcp/BestTextEditor_mcp_cat.h
catalog_translations := $(wildcard catalogs/*/BestTextEditor_mcp.po)

COMMON_CCFLAGS = -MP -MMD -Isupport -Iinclude -Isrc -Imcp -Wextra -Wno-unused-function \
	-Wno-discarded-qualifiers -Wno-int-conversion \
	-Wno-incompatible-pointer-types -Wno-unused-parameter
ifeq ($(DEBUG),1)
	COMMON_CCFLAGS += -g -Og
else
	COMMON_CCFLAGS += -Ofast
endif

.PHONY: all clean catalog

all: $(MCC) $(MCP)

clean:
	$(RM) $(MCC) $(MCC).debug $(MCC).map $(MCP) $(MCP).debug $(MCP).map
	$(RM) $(CATALOG_SOURCES) $(MCC_OBJECTS) $(MCP_OBJECTS)
	$(RM) $(MCC_OBJECTS:.o=.d) $(MCP_OBJECTS:.o=.d)

$(BUILD_DIR)/mcc $(BUILD_DIR)/mcp $(OUT_DIR):
	mkdir -p $@

catalog: $(CATALOG_SOURCES)
	@for po in $(catalog_translations); do \
		msgmerge -q -U $$po $(CATALOG_DEFINITION) || true; \
		flexcat POFILE $$po CATALOG $$(dirname $$po)/BestTextEditor_mcp.catalog || true; \
	done

mcp/BestTextEditor_mcp_cat.h: $(CATALOG_DEFINITION)
	flexcat $(CATALOG_DEFINITION) $@=C_h.sd

mcp/BestTextEditor_mcp_cat.c: $(CATALOG_DEFINITION) mcp/BestTextEditor_mcp_cat.h
	flexcat $(CATALOG_DEFINITION) $@=C_c.sd

# library.c defines the library bases itself (mccinit.c)
$(BUILD_DIR)/mcc/library.o: src/library.c | $(BUILD_DIR)/mcc
	$(CC) $(CCFLAGS) -D__NOLIBBASE__ -c -o $@ $<

$(BUILD_DIR)/mcc/%.o: src/%.c | $(BUILD_DIR)/mcc
	$(CC) $(CCFLAGS) -c -o $@ $<

$(BUILD_DIR)/mcp/library.o: mcp/library.c $(CATALOG_SOURCES) | $(BUILD_DIR)/mcp
	$(CC) $(CCFLAGS) -D__NOLIBBASE__ -c -o $@ $<

$(BUILD_DIR)/mcp/%.o: mcp/%.c $(CATALOG_SOURCES) | $(BUILD_DIR)/mcp
	$(CC) $(CCFLAGS) -c -o $@ $<

# The m68k strip writes its output without the executable bit, and the LhA
# archive carries the host mode over, so set it on every platform.
$(MCC): $(MCC_OBJECTS) | $(OUT_DIR)
	$(CC) $(LDFLAGS) -o $@.debug $(MCC_OBJECTS) $(LDLIBS) -Wl,-Map=$@.map
	$(STRIP) $(STRIPFLAGS) -o $@ $@.debug
	chmod 755 $@

$(MCP): $(MCP_OBJECTS) catalog | $(OUT_DIR)
	$(CC) $(LDFLAGS) -o $@.debug $(MCP_OBJECTS) $(LDLIBS) -Wl,-Map=$@.map
	$(STRIP) $(STRIPFLAGS) -o $@ $@.debug
	chmod 755 $@

-include $(MCC_OBJECTS:.o=.d) $(MCP_OBJECTS:.o=.d)
