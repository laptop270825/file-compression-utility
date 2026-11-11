CC     := gcc
CFLAGS := -std=c11 -Wall -Wextra -Wpedantic -O2 -Iinclude

SRC     := $(wildcard src/*.c)
OBJ     := $(patsubst src/%.c,build/%.o,$(SRC))
LIB_OBJ := $(filter-out build/main.o,$(OBJ))

ifeq ($(OS),Windows_NT)
    TARGET    := build/cmpz.exe
    TEST_BIN  := build/test_bitio.exe
    RUN_TEST  := build\test_bitio.exe
    MKBUILD   := mkdir build
    RMBUILD   := rmdir /s /q build
else
    TARGET    := build/cmpz
    TEST_BIN  := build/test_bitio
    RUN_TEST  := ./build/test_bitio
    MKBUILD   := mkdir -p build
    RMBUILD   := rm -rf build
endif

.PHONY: all test clean

all: $(TARGET)

$(TARGET): $(OBJ)
	$(CC) $(OBJ) -o $@

build/%.o: src/%.c | build
	$(CC) $(CFLAGS) -c $< -o $@

build/test_bitio.o: tests/test_bitio.c | build
	$(CC) $(CFLAGS) -c $< -o $@

$(TEST_BIN): build/test_bitio.o $(LIB_OBJ)
	$(CC) $^ -o $@

build:
	$(MKBUILD)

test: $(TEST_BIN)
	$(RUN_TEST)

clean:
	-$(RMBUILD)