CC     := gcc
CFLAGS := -std=c11 -Wall -Wextra -Wpedantic -O2 -Iinclude

SRC     := $(wildcard src/*.c)
OBJ     := $(patsubst src/%.c,build/%.o,$(SRC))
LIB_OBJ := $(filter-out build/main.o,$(OBJ))

TEST_SRC   := $(wildcard tests/test_*.c)
TEST_NAMES := $(patsubst tests/%.c,%,$(TEST_SRC))

ifeq ($(OS),Windows_NT)
    EXE     := .exe
    MKBUILD := mkdir build
    RMBUILD := rmdir /s /q build
    RUNPATH  = $(subst /,\,$(1))
else
    EXE     :=
    MKBUILD := mkdir -p build
    RMBUILD := rm -rf build
    RUNPATH  = ./$(1)
endif

TARGET    := build/cmpz$(EXE)
TEST_BINS := $(addprefix build/,$(addsuffix $(EXE),$(TEST_NAMES)))

.PHONY: all test e2e check clean

all: $(TARGET)

$(TARGET): $(OBJ)
	$(CC) $(OBJ) -o $@

build/%.o: src/%.c | build
	$(CC) $(CFLAGS) -c $< -o $@

# Each tests/test_X.c becomes its own program, linked with every module except main.c
build/test_%.o: tests/test_%.c | build
	$(CC) $(CFLAGS) -c $< -o $@

$(TEST_BINS): build/%$(EXE): build/%.o $(LIB_OBJ)
	$(CC) $^ -o $@

build:
	$(MKBUILD)

# End-to-end tests run the real program; tests/e2e_cli.c is a standalone program
build/e2e_cli.o: tests/e2e_cli.c | build
	$(CC) $(CFLAGS) -c $< -o $@

build/e2e_cli$(EXE): build/e2e_cli.o
	$(CC) $^ -o $@

test: $(addprefix run-,$(TEST_NAMES))

run-%: build/%$(EXE)
	$(call RUNPATH,build/$*$(EXE))

e2e: $(TARGET) build/e2e_cli$(EXE)
	$(call RUNPATH,build/e2e_cli$(EXE)) $(call RUNPATH,$(TARGET))

check: test e2e

clean:
	-$(RMBUILD)