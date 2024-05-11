# Define the compiler
CC=gcc

# Compiler flags
CFLAGS=-I/opt/homebrew/include -Wall -g

# Linker flags
LDFLAGS=-L/opt/homebrew/lib -ljson-c -lssl -lcrypto

# Define the source files
SOURCES=test.c

# Define the object files
OBJECTS=$(SOURCES:.c=.o)

# Define the executable file
EXECUTABLE=openai-test

BUILD_DIR := build/

$(BUILD_DIR):
	@$(info Creating directory $@)
	@mkdir -p $@

all: $(EXECUTABLE)

$(EXECUTABLE): $(OBJECTS) 
	$(CC) $(LDFLAGS) $(OBJECTS) -o $@

%.o: %.c | $(BUILD_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -f $(OBJECTS) $(EXECUTABLE)

.PHONY: all clean
