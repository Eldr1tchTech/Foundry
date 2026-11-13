# =============================================================================
# Foundry - project template Makefile
#
# Layout:
#   include/   public headers
#   src/       library sources (*.c, searched recursively)
#   tests/     unit tests (*.c, searched recursively; must provide main())
#   build/     all build output (build/debug, build/release) - safe to delete
#
# Targets:
#   make               same as `make debug`
#   make debug         build tests with ASan + UBSan + LeakSan, no optimisation
#   make release       build tests with -O3 (+ LTO, -march=native)
#   make test          build debug and run the tests
#   make test-release  build release and run the tests
#   make clean         remove everything produced by the build
#   make help          show this list
#
# Overrides (all optional):
#   make CC=gcc                 use another compiler
#   make release NATIVE=0       drop -march=native (portable binary)
#   make release LTO=0          drop link-time optimisation
#   make debug WERROR=1         treat warnings as errors
# =============================================================================

NAME      := foundry

SRC_DIR   := src
INC_DIR   := include
TEST_DIR  := tests
BUILD_ROOT := build

# Default to clang; `make CC=gcc` or `CC=gcc make` still wins.
ifeq ($(origin CC),default)
CC := clang
endif
AR ?= ar

CSTD     ?= -std=c23
NATIVE   ?= 1
LTO      ?= 1
WERROR   ?= 0

# -----------------------------------------------------------------------------
# Build mode (set by the debug/release targets via a recursive make)
# -----------------------------------------------------------------------------
MODE ?= debug
BUILD_DIR := $(BUILD_ROOT)/$(MODE)

WARNINGS := -Wall -Wextra -Wpedantic -Wshadow -Wconversion -Wsign-conversion \
            -Wformat=2 -Wnull-dereference -Wundef -Wstrict-prototypes
ifeq ($(WERROR),1)
WARNINGS += -Werror
endif

CPPFLAGS := -I$(INC_DIR) -MMD -MP
CFLAGS   := $(CSTD) $(WARNINGS)
LDFLAGS  :=
LDLIBS   :=

ifeq ($(MODE),debug)
  SANITIZE := -fsanitize=address,undefined,leak
  CPPFLAGS += -DDEBUG
  CFLAGS   += -O0 -g3 -fno-omit-frame-pointer -fno-optimize-sibling-calls \
              -fstack-protector-strong $(SANITIZE) -fno-sanitize-recover=all
  LDFLAGS  += $(SANITIZE)
else ifeq ($(MODE),release)
  CPPFLAGS += -DNDEBUG
  CFLAGS   += -O3 -fomit-frame-pointer
  ifeq ($(NATIVE),1)
    CFLAGS += -march=native
  endif
  ifeq ($(LTO),1)
    CFLAGS  += -flto
    LDFLAGS += -flto
  endif
else
  $(error Unknown MODE '$(MODE)' - use the `debug` or `release` targets)
endif

# -----------------------------------------------------------------------------
# Files
# -----------------------------------------------------------------------------
LIB_SRCS  := $(shell find $(SRC_DIR) -name '*.c' 2>/dev/null)
TEST_SRCS := $(shell find $(TEST_DIR) -name '*.c' 2>/dev/null)

LIB_OBJS  := $(LIB_SRCS:%.c=$(BUILD_DIR)/%.o)
TEST_OBJS := $(TEST_SRCS:%.c=$(BUILD_DIR)/%.o)
DEPS      := $(LIB_OBJS:.o=.d) $(TEST_OBJS:.o=.d)

LIB       := $(BUILD_DIR)/lib$(NAME).a
TEST_BIN  := $(BUILD_DIR)/$(NAME)_tests

# Make leak detection explicit and give readable UBSan reports when running.
SAN_ENV := ASAN_OPTIONS=detect_leaks=1:strict_string_checks=1:check_initialization_order=1:detect_stack_use_after_return=1 \
           UBSAN_OPTIONS=print_stacktrace=1:halt_on_error=1

# -----------------------------------------------------------------------------
# Public targets
# -----------------------------------------------------------------------------
.DEFAULT_GOAL := debug
.PHONY: debug release test test-release clean help build

debug:
	@$(MAKE) --no-print-directory MODE=debug build

release:
	@$(MAKE) --no-print-directory MODE=release build

test: debug
	@echo "==> running debug tests"
	@$(SAN_ENV) $(BUILD_ROOT)/debug/$(NAME)_tests

test-release: release
	@echo "==> running release tests"
	@$(BUILD_ROOT)/release/$(NAME)_tests

clean:
	rm -rf $(BUILD_ROOT)

help:
	@sed -n '/^# Targets:/,/^# =====/p' $(firstword $(MAKEFILE_LIST)) | sed '$$d' | sed 's/^# \{0,1\}//'

# -----------------------------------------------------------------------------
# Internal build rules (run with MODE set)
# -----------------------------------------------------------------------------
# The static library is always produced; the test binary links the objects
# directly so that LTO works without needing a compiler-specific `ar` plugin.
build: $(LIB) $(TEST_BIN)

$(LIB): $(LIB_OBJS)
	@mkdir -p $(@D)
	$(AR) rcs $@ $^

$(TEST_BIN): $(TEST_OBJS) $(LIB_OBJS)
	@mkdir -p $(@D)
	$(CC) $(CFLAGS) $(LDFLAGS) $^ -o $@ $(LDLIBS)

$(BUILD_DIR)/%.o: %.c
	@mkdir -p $(@D)
	$(CC) $(CPPFLAGS) $(CFLAGS) -c $< -o $@

-include $(DEPS)