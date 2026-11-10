CC     := gcc
CFLAGS := -std=c11 -Wall -Wextra -Wpedantic -O2 -Iinclude

SRC := $(wildcard src/*.c)
OBJ := $(patsubst src/%.c,build/%.o,$(SRC))

ifeq ($(OS),Windows_NT)
    TARGET  := build/cmpz.exe
    MKBUILD := mkdir build
    RMBUILD := rmdir /s /q build
else
    TARGET  := build/cmpz
    MKBUILD := mkdir -p build
    RMBUILD := rm -rf build
endif

.PHONY: all test clean

all: $(TARGET)

$(TARGET): $(OBJ)
	$(CC) $(OBJ) -o $@

build/%.o: src/%.c | build
	$(CC) $(CFLAGS) -c $< -o $@

build:
	$(MKBUILD)

test: all
	@echo No tests yet.

clean:
	-$(RMBUILD)