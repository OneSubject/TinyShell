# TinyShell Makefile
# Default target is 'help'. Just type `make`.

CC      ?= cc
CFLAGS  ?= -Wall -Wextra -std=c11 -O2 -g
LDFLAGS ?=

# Debug build with sanitizers: make debug
DBG_CFLAGS  ?= -Wall -Wextra -std=c11 -O0 -g3 -fsanitize=address,undefined
DBG_LDFLAGS ?= -fsanitize=address,undefined

COLOR_OK    := \033[32m
COLOR_INFO  := \033[36m
COLOR_WARN  := \033[33m
COLOR_RESET := \033[0m

SRC_DIR := src
BUILD_DIR := build
TARGET := $(BUILD_DIR)/tinyshell

SRCS := $(wildcard $(SRC_DIR)/*.c)
OBJS := $(patsubst $(SRC_DIR)/%.c,$(BUILD_DIR)/%.o,$(SRCS))
DEPS := $(OBJS:.o=.d)

.PHONY: build clean run debug help
.DEFAULT_GOAL := help

build: clean $(TARGET)

$(TARGET): $(OBJS)
	@echo "$(COLOR_OK)[LINK]$(COLOR_RESET) $@"
	$(CC) $(LDFLAGS) -o $@ $(OBJS)

$(BUILD_DIR)/%.o: $(SRC_DIR)/%.c
	@mkdir -p $(dir $@)
	@echo "$(COLOR_OK)[CC]$(COLOR_RESET) $<"
	$(CC) $(CFLAGS) -MMD -MP -c $< -o $@

-include $(DEPS)

run: $(TARGET)
	@echo "$(COLOR_INFO)[RUN]$(COLOR_RESET)"
	./$(TARGET)

debug: CFLAGS  = $(DBG_CFLAGS)
debug: LDFLAGS = $(DBG_LDFLAGS)
debug: clean $(TARGET)
	@echo "$(COLOR_WARN)[DEBUG]$(COLOR_RESET) starting gdb with sanitizers"
	gdb --args ./$(TARGET)

clean:
	@echo "$(COLOR_WARN)[CLEAN]$(COLOR_RESET) $(BUILD_DIR)"
	rm -rf $(BUILD_DIR)

help:
	@echo ""
	@echo "$(COLOR_INFO)TinyShell targets:$(COLOR_RESET)"
	@echo ""
	@echo "  $(COLOR_OK)make$(COLOR_RESET)          - show this help (default)"
	@echo "  $(COLOR_OK)make build$(COLOR_RESET)    - compile the shell"
	@echo "  $(COLOR_OK)make run$(COLOR_RESET)      - build and run the shell"
	@echo "  $(COLOR_OK)make debug$(COLOR_RESET)    - build with ASan/UBSan and run under gdb"
	@echo "  $(COLOR_OK)make clean$(COLOR_RESET)    - remove build directory"
	@echo ""
