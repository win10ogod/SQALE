PROJECT := sqale
BUILD_DIR := build
SRC_DIR := src
INC_DIR := include

CC ?= cc
CFLAGS ?= -std=c11 -O2 -g -Wall -Wextra -Werror -fno-strict-aliasing -I$(INC_DIR)
LDFLAGS ?=

# Set USE_LLVM=1 to enable LLVM backend (requires llvm-config in PATH)
USE_LLVM ?= 0
LLVM_CONFIG ?= llvm-config

OS := $(shell uname -s)

ifeq ($(OS),Linux)
  PLATFORM_LIBS := -lpthread -ldl
else ifeq ($(OS),Darwin)
  PLATFORM_LIBS := -lpthread
else
  # Windows (MSYS/MinGW): threads are built-in; no -lpthread
  PLATFORM_LIBS :=
endif

SRCS := \
  $(SRC_DIR)/main.c \
  $(SRC_DIR)/arena.c $(SRC_DIR)/str.c $(SRC_DIR)/vec.c \
  $(SRC_DIR)/lexer.c $(SRC_DIR)/parser.c \
  $(SRC_DIR)/ast.c $(SRC_DIR)/type.c $(SRC_DIR)/env.c \
  $(SRC_DIR)/gc.c $(SRC_DIR)/value.c $(SRC_DIR)/runtime.c \
  $(SRC_DIR)/thread.c $(SRC_DIR)/channel.c \
  $(SRC_DIR)/eval.c $(SRC_DIR)/codegen_llvm.c $(SRC_DIR)/macro.c \
  $(SRC_DIR)/repl.c

OBJS := $(SRCS:$(SRC_DIR)/%.c=$(BUILD_DIR)/%.o)
BIN := $(BUILD_DIR)/$(PROJECT)

ifeq ($(USE_LLVM),1)
  LLVM_CFLAGS := $(shell $(LLVM_CONFIG) --cflags)
  LLVM_LDFLAGS := $(shell $(LLVM_CONFIG) --ldflags --libs core native orcjit mcjit nativecodegen)
  CFLAGS += -DUSE_LLVM=1 $(LLVM_CFLAGS)
  LDFLAGS += $(LLVM_LDFLAGS)
else
  CFLAGS += -DUSE_LLVM=0
endif

.PHONY: all clean run repl

all: $(BIN)

$(BUILD_DIR):
	mkdir -p $(BUILD_DIR)

$(BUILD_DIR)/%.o: $(SRC_DIR)/%.c | $(BUILD_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

$(BIN): $(OBJS)
	$(CC) $(CFLAGS) $^ -o $@ $(LDFLAGS) $(PLATFORM_LIBS)

run: $(BIN)
	$(BIN) run examples/hello.sq

repl: $(BIN)
	$(BIN) repl

clean:
	rm -rf $(BUILD_DIR)

# Windows packaging (builds sqale.exe via CC if available)
.PHONY: win package-win
win: $(BIN)
	@echo "To produce Windows exe, set CC='x86_64-w64-mingw32-gcc' or clang with -target and rebuild."

package-win: $(BIN)
	@mkdir -p $(BUILD_DIR)/win
	@cp $(BIN) $(BUILD_DIR)/win/sqale.exe || cp $(BIN) $(BUILD_DIR)/win/sqale
	@cd $(BUILD_DIR) && zip -r sqale-win.zip win >/dev/null 2>&1 || true
