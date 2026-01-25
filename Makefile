#
# SQLite/CE Master Build System
#
# This Makefile builds SQLite/CE components for desktop Linux/Windows.
# The primary target (Windows CE) uses Visual C++ 6.0 project files.
#
# Usage:
#   make            - Build all components
#   make sqlite     - Build SQLite library only
#   make test       - Build and run unit tests
#   make bench      - Build benchmark tool
#   make clean      - Remove build artifacts
#   make help       - Show all targets
#
# Configuration:
#   DEBUG=1         - Build with debug symbols
#   VERBOSE=1       - Show compiler commands
#   CC=gcc          - Override compiler
#

#============================================================================
# Configuration
#============================================================================

# Directories
SRCDIR      := src
BUILDDIR    := build
BINDIR      := $(BUILDDIR)/bin
LIBDIR      := $(BUILDDIR)/lib
OBJDIR      := $(BUILDDIR)/obj

# Source directories
SQLITE_SRC     := $(SRCDIR)/sqlite
SQLITE_CE_SRC  := $(SRCDIR)/sqlite-ce
EDIT_SRC       := $(SRCDIR)/sqlite-ce-edit
TEST_SRC       := $(SRCDIR)/sqlite-ce-test
BENCH_SRC      := $(SRCDIR)/sqlite-ce-bench

# Compiler settings
CC          ?= gcc
AR          ?= ar
CFLAGS      := -Wall -Wextra -Wno-unused-parameter
CFLAGS      += -I$(SQLITE_SRC) -I$(SQLITE_CE_SRC) -I$(EDIT_SRC)
LDFLAGS     :=
LIBS        := -lm

# Debug vs Release
ifdef DEBUG
    CFLAGS  += -g -O0 -DDEBUG -DSQLITE_DEBUG
    BUILD_TYPE := debug
else
    CFLAGS  += -O2 -DNDEBUG
    BUILD_TYPE := release
endif

# Desktop build defines (not Windows CE)
CFLAGS      += -DSQLITE_DESKTOP_BUILD
CFLAGS      += -DOS_UNIX=1 -DOS_WIN=0 -DOS_WINCE=0 -DOS_MAC=0

# Verbose output
ifndef VERBOSE
    Q := @
endif

#============================================================================
# SQLite Core Library Sources
#============================================================================

SQLITE_CORE_SRCS := \
    $(SQLITE_SRC)/attach.c \
    $(SQLITE_SRC)/auth.c \
    $(SQLITE_SRC)/btree.c \
    $(SQLITE_SRC)/btree_rb.c \
    $(SQLITE_SRC)/build.c \
    $(SQLITE_SRC)/copy.c \
    $(SQLITE_SRC)/date.c \
    $(SQLITE_SRC)/delete.c \
    $(SQLITE_SRC)/encode.c \
    $(SQLITE_SRC)/expr.c \
    $(SQLITE_SRC)/func.c \
    $(SQLITE_SRC)/hash.c \
    $(SQLITE_SRC)/insert.c \
    $(SQLITE_SRC)/main.c \
    $(SQLITE_SRC)/pager.c \
    $(SQLITE_SRC)/parse.c \
    $(SQLITE_SRC)/pragma.c \
    $(SQLITE_SRC)/printf.c \
    $(SQLITE_SRC)/random.c \
    $(SQLITE_SRC)/select.c \
    $(SQLITE_SRC)/table.c \
    $(SQLITE_SRC)/tokenize.c \
    $(SQLITE_SRC)/trigger.c \
    $(SQLITE_SRC)/update.c \
    $(SQLITE_SRC)/util.c \
    $(SQLITE_SRC)/vacuum.c \
    $(SQLITE_SRC)/vdbe.c \
    $(SQLITE_SRC)/vdbeaux.c \
    $(SQLITE_SRC)/where.c

# Note: parse.c should be generated from parse.y using lemon
# For now we assume it exists

SQLITE_CORE_OBJS := $(patsubst $(SRCDIR)/%.c,$(OBJDIR)/%.o,$(SQLITE_CORE_SRCS))

#============================================================================
# Test Sources
#============================================================================

TEST_SRCS := \
    $(TEST_SRC)/test_main.c

TEST_OBJS := $(patsubst $(SRCDIR)/%.c,$(OBJDIR)/%.o,$(TEST_SRCS))

#============================================================================
# Benchmark Sources
#============================================================================

BENCH_SRCS := \
    $(BENCH_SRC)/bench_main.c

BENCH_OBJS := $(patsubst $(SRCDIR)/%.c,$(OBJDIR)/%.o,$(BENCH_SRCS))

#============================================================================
# Targets
#============================================================================

.PHONY: all sqlite test bench clean help dirs check verify

all: sqlite test bench

sqlite: dirs $(LIBDIR)/libsqlite.a
	@echo "Built SQLite library: $(LIBDIR)/libsqlite.a"

test: dirs $(BINDIR)/sqlite_test
	@echo "Running tests..."
	$(Q)$(BINDIR)/sqlite_test

bench: dirs $(BINDIR)/sqlite_bench
	@echo "Built benchmark: $(BINDIR)/sqlite_bench"

clean:
	@echo "Cleaning build artifacts..."
	$(Q)rm -rf $(BUILDDIR)

check:
	@echo "Running static analysis with cppcheck..."
	$(Q)cppcheck --enable=all --suppress-xml=cppcheck-suppress.xml \
		-I$(SQLITE_SRC) -I$(SQLITE_CE_SRC) -I$(EDIT_SRC) \
		--platform=win32W \
		--template="{file}:{line}: {severity}: {message} [{id}]" \
		--suppress=missingIncludeSystem \
		--suppress=unusedFunction:$(SQLITE_SRC)/*.c \
		$(SQLITE_SRC) $(SQLITE_CE_SRC) 2>&1 | head -50
	@echo ""
	@echo "Static analysis complete. For full output, run:"
	@echo "  cppcheck --enable=all --suppress-xml=cppcheck-suppress.xml src/"

verify:
	@echo "Running full build verification..."
	$(Q)./scripts/build-verify.sh

help:
	@echo "SQLite/CE Build System"
	@echo ""
	@echo "Targets:"
	@echo "  all      - Build everything (default)"
	@echo "  sqlite   - Build SQLite library"
	@echo "  test     - Build and run unit tests"
	@echo "  bench    - Build benchmark tool"
	@echo "  check    - Run static analysis (cppcheck)"
	@echo "  verify   - Full build verification (clean, build, test, check)"
	@echo "  clean    - Remove build artifacts"
	@echo ""
	@echo "Options:"
	@echo "  DEBUG=1   - Build with debug symbols"
	@echo "  VERBOSE=1 - Show compiler commands"
	@echo ""
	@echo "Note: This Makefile builds for desktop Linux/Windows."
	@echo "For Windows CE targets, use Visual C++ 6.0 projects."

#============================================================================
# Directory Creation
#============================================================================

dirs:
	$(Q)mkdir -p $(BINDIR)
	$(Q)mkdir -p $(LIBDIR)
	$(Q)mkdir -p $(OBJDIR)/sqlite
	$(Q)mkdir -p $(OBJDIR)/sqlite-ce
	$(Q)mkdir -p $(OBJDIR)/sqlite-ce-edit
	$(Q)mkdir -p $(OBJDIR)/sqlite-ce-test
	$(Q)mkdir -p $(OBJDIR)/sqlite-ce-bench

#============================================================================
# Library Build
#============================================================================

$(LIBDIR)/libsqlite.a: $(SQLITE_CORE_OBJS)
	@echo "AR $@"
	$(Q)$(AR) rcs $@ $^

#============================================================================
# Test Executable
#============================================================================

$(BINDIR)/sqlite_test: $(TEST_OBJS) $(LIBDIR)/libsqlite.a
	@echo "LD $@"
	$(Q)$(CC) $(LDFLAGS) -o $@ $(TEST_OBJS) -L$(LIBDIR) -lsqlite $(LIBS)

#============================================================================
# Benchmark Executable
#============================================================================

$(BINDIR)/sqlite_bench: $(BENCH_OBJS) $(LIBDIR)/libsqlite.a
	@echo "LD $@"
	$(Q)$(CC) $(LDFLAGS) -o $@ $(BENCH_OBJS) -L$(LIBDIR) -lsqlite $(LIBS)

#============================================================================
# Pattern Rules
#============================================================================

$(OBJDIR)/%.o: $(SRCDIR)/%.c
	@echo "CC $<"
	$(Q)mkdir -p $(dir $@)
	$(Q)$(CC) $(CFLAGS) -c $< -o $@

#============================================================================
# Dependencies
#============================================================================

# Auto-generate dependencies
DEPS := $(SQLITE_CORE_OBJS:.o=.d) $(TEST_OBJS:.o=.d) $(BENCH_OBJS:.o=.d)

$(OBJDIR)/%.d: $(SRCDIR)/%.c
	$(Q)mkdir -p $(dir $@)
	$(Q)$(CC) $(CFLAGS) -MM -MT '$(OBJDIR)/$*.o' $< > $@

-include $(DEPS)
