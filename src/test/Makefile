CC=gcc

BUILD_DIR := ../../build/test/obj/

SUBDIRS := $(wildcard ./*/)

C_SOURCES := $(wildcard *.c) $(foreach dir, $(SUBDIRS), $(wildcard $(dir)/*.c))
C_OBJECTS := $(addprefix $(BUILD_DIR), $(notdir $(C_SOURCES:.c=.o)))

LDFLAGS = -L/opt/homebrew/lib -ljson-c -lssl -lcrypto
CFLAGS = -I/opt/homebrew/include -Wall -g -O0

OBJECTS := $(C_OBJECTS)

EXECUTABLE_OUT = ../../out/openai-test

.PHONY: all clean

all: $(EXECUTABLE_OUT)

$(BUILD_DIR):
	@$(info Creating directory $@)
	@mkdir -p $@

$(EXECUTABLE_OUT): $(OBJECTS) | openai-key.h
	@$(info Linking $@)
	$(CC) $(CFLAGS) $(LDFLAGS) $(OBJECTS) -o $@

openai-key.h:
	cp openai-key.h.template openai-key.h

$(C_OBJECTS): $(BUILD_DIR)%.o: %.c | $(BUILD_DIR)
	@$(info Compiling $<)
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -f $(OBJECTS) $(EXECUTABLE_OUT)
