# TinyShell Makefile
# Default target is 'help'. Just type `make`.
#
# INFRASTRUCTURE.md explains the reasoning behind every target here.

# Linux only. The tests read /proc and use GNU ps and env, and the comments
# and the book describe glibc's behaviour. Better to stop here than to fail
# in a confusing way halfway through a test run.
UNAME := $(shell uname -s)
ifneq ($(UNAME),Linux)
  $(error TinyShell only supports Linux (uname -s says '$(UNAME)'))
endif

CC      ?= cc
CFLAGS  ?= -std=c11 -O2 -g -Wall -Wextra -Wpedantic -Wshadow -Wstrict-prototypes
DEFS    ?=
LDFLAGS ?=

# fork(), execv(), chdir() and friends are POSIX, not C11. -std=c11 on its own
# puts the compiler in strict-ANSI mode, where every non-standard declaration
# is hidden, and the errors that produces ("implicit declaration of function
# 'fork'") are among the most confusing a beginner hits in C. So the feature
# test macro is requested explicitly and kept here where it can be read.
DEFS += -D_POSIX_C_SOURCE=200809L

# The debug build: no optimisation to confuse gdb, full debug info, and the
# address + undefined-behaviour sanitizers. It lives in its own directory so
# its objects can never be mixed with the normal build's.
DEBUG_CFLAGS  := -std=c11 -Og -g3 -Wall -Wextra -Wpedantic -Wshadow \
                 -Wstrict-prototypes -fno-omit-frame-pointer \
                 -fsanitize=address,undefined
DEBUG_LDFLAGS := -fsanitize=address,undefined

# Which milestones `make test` holds you to. Tests for later milestones still
# run, but report TODO instead of FAIL. Bump this as you finish each one,
# or pass it on the command line: `make test MILESTONE=4`.
MILESTONE ?= 3

# Where `make install` puts the binary. ~/.local/bin needs no sudo and is on
# PATH in most distributions; `make install PREFIX=/usr/local` for system-wide.
PREFIX ?= $(HOME)/.local

CLANG_FORMAT ?= clang-format
CPPCHECK     ?= cppcheck
MDBOOK       ?= mdbook
BOOK_DIR     := docs

# `make V=1` prints the full compiler command lines.
V ?= 0
ifeq ($(V),1)
  Q :=
else
  Q := @
endif

COLOR_OK    := \033[32m
COLOR_INFO  := \033[36m
COLOR_WARN  := \033[33m
COLOR_ERR   := \033[31m
COLOR_RESET := \033[0m

SRC_DIR   := src
TEST_DIR  := tests
BUILD_DIR := build
OBJ_DIR   := $(BUILD_DIR)/obj
DEBUG_DIR := $(BUILD_DIR)/debug

TARGET       := $(BUILD_DIR)/tinyshell
DEBUG_TARGET := $(DEBUG_DIR)/tinyshell
UNIT_TARGET  := $(BUILD_DIR)/unit_tests

SRCS       := $(wildcard $(SRC_DIR)/*.c)
HDRS       := $(wildcard $(SRC_DIR)/*.h)
OBJS       := $(patsubst $(SRC_DIR)/%.c,$(OBJ_DIR)/%.o,$(SRCS))
DEBUG_OBJS := $(patsubst $(SRC_DIR)/%.c,$(DEBUG_DIR)/%.o,$(SRCS))

# The unit tests link against every object except the one holding main().
LIB_OBJS        := $(filter-out $(OBJ_DIR)/tinyshell.o,$(OBJS))
UNIT_TEST_SRCS  := $(wildcard $(TEST_DIR)/*.c)
UNIT_TEST_OBJS  := $(patsubst $(TEST_DIR)/%.c,$(OBJ_DIR)/tests/%.o,$(UNIT_TEST_SRCS))

DEPS := $(OBJS:.o=.d) $(DEBUG_OBJS:.o=.d) $(UNIT_TEST_OBJS:.o=.d)

.PHONY: all build run debug gdb test test-unit test-shell test-debug \
        fmt fmt-check lint book book-serve install uninstall clean help
.DEFAULT_GOAL := help

all: build

build: $(TARGET)

$(TARGET): $(OBJS)
	@printf '$(COLOR_OK)[LINK]$(COLOR_RESET) %s\n' $@
	$(Q)$(CC) $(LDFLAGS) -o $@ $(OBJS)

$(OBJ_DIR)/%.o: $(SRC_DIR)/%.c
	@mkdir -p $(dir $@)
	@printf '$(COLOR_OK)[CC]$(COLOR_RESET) %s\n' $<
	$(Q)$(CC) $(DEFS) $(CFLAGS) -MMD -MP -c $< -o $@

run: $(TARGET)
	@printf '$(COLOR_INFO)[RUN]$(COLOR_RESET) %s  (Ctrl-D or `exit` to quit)\n' $(TARGET)
	@./$(TARGET)

# debug: sanitizers + gdb

$(DEBUG_TARGET): $(DEBUG_OBJS)
	@printf '$(COLOR_OK)[LINK]$(COLOR_RESET) %s (asan+ubsan)\n' $@
	$(Q)$(CC) $(DEBUG_LDFLAGS) -o $@ $(DEBUG_OBJS)

$(DEBUG_DIR)/%.o: $(SRC_DIR)/%.c
	@mkdir -p $(dir $@)
	@printf '$(COLOR_OK)[CC]$(COLOR_RESET) %s (debug)\n' $<
	$(Q)$(CC) $(DEFS) $(DEBUG_CFLAGS) -MMD -MP -c $< -o $@

# Run the sanitized build directly. Memory errors and undefined behaviour
# stop the shell with a report pointing at the line responsible.
debug: $(DEBUG_TARGET)
	@printf '$(COLOR_INFO)[DEBUG]$(COLOR_RESET) %s  (AddressSanitizer + UBSan)\n' $(DEBUG_TARGET)
	@UBSAN_OPTIONS=print_stacktrace=1 ./$(DEBUG_TARGET)

# The same binary under gdb. LeakSanitizer refuses to run under ptrace, so
# leak checking is switched off here; `make debug` still has it.
gdb: $(DEBUG_TARGET)
	@command -v gdb >/dev/null 2>&1 || { \
	  printf '$(COLOR_ERR)[GDB]$(COLOR_RESET) gdb is not installed\n'; exit 1; }
	@printf '$(COLOR_INFO)[GDB]$(COLOR_RESET) %s\n' $(DEBUG_TARGET)
	@ASAN_OPTIONS=detect_leaks=0 gdb -q $(DEBUG_TARGET)

# tests

test: test-unit test-shell

test-unit: $(UNIT_TARGET)
	@printf '$(COLOR_INFO)[TEST]$(COLOR_RESET) unit tests\n'
	@./$(UNIT_TARGET)

test-shell: $(TARGET)
	@printf '$(COLOR_INFO)[TEST]$(COLOR_RESET) shell tests (MILESTONE=%s)\n' $(MILESTONE)
	@sh $(TEST_DIR)/run_tests.sh $(TARGET) $(MILESTONE)

# The shell tests again, against the sanitized build. Slower; worth running
# after any change to buffers, file descriptors or child processes.
test-debug: $(DEBUG_TARGET)
	@printf '$(COLOR_INFO)[TEST]$(COLOR_RESET) shell tests on the sanitized build (MILESTONE=%s)\n' $(MILESTONE)
	@UBSAN_OPTIONS=print_stacktrace=1:halt_on_error=1 \
	  sh $(TEST_DIR)/run_tests.sh $(DEBUG_TARGET) $(MILESTONE)

$(UNIT_TARGET): $(UNIT_TEST_OBJS) $(LIB_OBJS)
	@printf '$(COLOR_OK)[LINK]$(COLOR_RESET) %s\n' $@
	$(Q)$(CC) $(LDFLAGS) -o $@ $(UNIT_TEST_OBJS) $(LIB_OBJS)

$(OBJ_DIR)/tests/%.o: $(TEST_DIR)/%.c
	@mkdir -p $(dir $@)
	@printf '$(COLOR_OK)[CC]$(COLOR_RESET) %s\n' $<
	$(Q)$(CC) $(DEFS) $(CFLAGS) -I$(SRC_DIR) -MMD -MP -c $< -o $@

# Header dependencies, generated by -MMD above.
-include $(DEPS)

# formatting and linting

FORMAT_FILES := $(SRCS) $(HDRS) $(UNIT_TEST_SRCS)

fmt:
	@command -v $(CLANG_FORMAT) >/dev/null 2>&1 || { \
	  printf '$(COLOR_ERR)[FMT]$(COLOR_RESET) %s not found (apt install clang-format)\n' $(CLANG_FORMAT); exit 1; }
	@printf '$(COLOR_INFO)[FMT]$(COLOR_RESET) %s\n' "$(FORMAT_FILES)"
	$(Q)$(CLANG_FORMAT) -i $(FORMAT_FILES)

# Same check, but only reports. Changes nothing, fails if anything would.
fmt-check:
	@command -v $(CLANG_FORMAT) >/dev/null 2>&1 || { \
	  printf '$(COLOR_ERR)[FMT]$(COLOR_RESET) %s not found (apt install clang-format)\n' $(CLANG_FORMAT); exit 1; }
	$(Q)$(CLANG_FORMAT) --dry-run --Werror $(FORMAT_FILES)

lint:
	@command -v $(CPPCHECK) >/dev/null 2>&1 || { \
	  printf '$(COLOR_ERR)[LINT]$(COLOR_RESET) %s not found (apt install cppcheck)\n' $(CPPCHECK); exit 1; }
	@printf '$(COLOR_INFO)[LINT]$(COLOR_RESET) cppcheck\n'
	$(Q)$(CPPCHECK) --quiet --std=c11 --error-exitcode=1 --inline-suppr \
	  --enable=warning,style,performance,portability \
	  --suppress=missingIncludeSystem \
	  -D_POSIX_C_SOURCE=200809L -I$(SRC_DIR) $(SRC_DIR) $(TEST_DIR)

# the book (docs/), built with mdBook, the tool behind the Rust book

MDBOOK_MISSING := printf '$(COLOR_ERR)[BOOK]$(COLOR_RESET) %s not found. install them with\n       `cargo install mdbook mdbook-mermaid`, or read docs/src/SUMMARY.md\n       as plain markdown.\n'

BOOK_TOOLS_CHECK := for tool in $(MDBOOK) mdbook-mermaid; do \
	  command -v $$tool >/dev/null 2>&1 || { $(MDBOOK_MISSING) $$tool; exit 1; }; \
	done

book:
	@$(BOOK_TOOLS_CHECK)
	@printf '$(COLOR_INFO)[BOOK]$(COLOR_RESET) build/book/index.html\n'
	$(Q)$(MDBOOK) build $(BOOK_DIR)

book-serve:
	@$(BOOK_TOOLS_CHECK)
	@printf '$(COLOR_INFO)[BOOK]$(COLOR_RESET) http://localhost:3000  (Ctrl-C to stop)\n'
	$(Q)$(MDBOOK) serve $(BOOK_DIR) --open

# install

install: $(TARGET)
	@printf '$(COLOR_INFO)[INSTALL]$(COLOR_RESET) %s\n' $(DESTDIR)$(PREFIX)/bin/tinyshell
	$(Q)install -d $(DESTDIR)$(PREFIX)/bin
	$(Q)install -m 755 $(TARGET) $(DESTDIR)$(PREFIX)/bin/tinyshell

uninstall:
	@printf '$(COLOR_WARN)[UNINSTALL]$(COLOR_RESET) %s\n' $(DESTDIR)$(PREFIX)/bin/tinyshell
	$(Q)rm -f $(DESTDIR)$(PREFIX)/bin/tinyshell

clean:
	@printf '$(COLOR_WARN)[CLEAN]$(COLOR_RESET) %s\n' $(BUILD_DIR)
	$(Q)rm -rf $(BUILD_DIR)

help:
	@printf '\n$(COLOR_INFO)TinyShell targets:$(COLOR_RESET)\n\n'
	@printf '  $(COLOR_OK)make$(COLOR_RESET)              show this help (default)\n'
	@printf '  $(COLOR_OK)make build$(COLOR_RESET)        compile to $(TARGET)\n'
	@printf '  $(COLOR_OK)make run$(COLOR_RESET)          build and start the shell\n'
	@printf '  $(COLOR_OK)make test$(COLOR_RESET)         unit + shell tests  (MILESTONE=$(MILESTONE); try MILESTONE=4)\n'
	@printf '  $(COLOR_OK)make test-debug$(COLOR_RESET)   shell tests on the sanitized build\n'
	@printf '  $(COLOR_OK)make debug$(COLOR_RESET)        run the shell with ASan + UBSan\n'
	@printf '  $(COLOR_OK)make gdb$(COLOR_RESET)          the sanitized shell under gdb\n'
	@printf '  $(COLOR_OK)make fmt$(COLOR_RESET)          clang-format every source file\n'
	@printf '  $(COLOR_OK)make lint$(COLOR_RESET)         static analysis with cppcheck\n'
	@printf '  $(COLOR_OK)make book$(COLOR_RESET)         build the book (docs/) into build/book/\n'
	@printf '  $(COLOR_OK)make book-serve$(COLOR_RESET)   read the book at http://localhost:3000\n'
	@printf '  $(COLOR_OK)make install$(COLOR_RESET)      copy to $(PREFIX)/bin  (PREFIX=... to change)\n'
	@printf '  $(COLOR_OK)make clean$(COLOR_RESET)        remove $(BUILD_DIR)/\n'
	@printf '\n$(COLOR_INFO)start here:$(COLOR_RESET) `make book-serve`, or docs/src/SUMMARY.md.\n\n'
