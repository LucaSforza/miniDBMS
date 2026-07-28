# miniDBMS — top-level Makefile
#
# Builds the MiniDBMS executable, the pinned Hyrise SQL parser, and
# the project test runners.
# No CMake or Curses required.
#
# == Usage ==
#   make              — incremental build (default target)
#   make test         — build and run the green (passing) test runner
#   make test-future  — build and run the future SQL specification runner
#   make test-objs    — compile all test objects without linking or running
#                       (useful for compile-commands capture)
#   make test-clean   — remove test build artifacts only
#   make clean        — remove all generated output
#   make compdb       — clean rebuild under Bear, writes root compile_commands.json
#
# == Output layout ==
#   build/
#     MiniDBMS          — the executable
#     libsqlparser.so   — Hyrise SQL parser shared library (copied)
#     libStorageEngine.a
#     libSQLInterpreter.a
#     obj/              — production object files and generated dependency files (*.d)
#     tests/            — test object files, binaries, and dependency files

CXX       := g++
CC        := gcc
AR        := ar

CXXFLAGS  := -std=c++17 -O2 -Wall -MMD -MP
CFLAGS    := -O2 -Wall -MMD -MP
INCLUDES  := -Iinclude -Ilibs/sql-parser/src -Ilibs/linenoise
LDFLAGS   := -Lbuild -Wl,-rpath,'$$ORIGIN'
LDLIBS    := -lsqlparser

PARSER_DIR    := libs/sql-parser
PARSER_LIB    := $(PARSER_DIR)/libsqlparser.so
BUILD_PARSER  := build/libsqlparser.so

# Parser compatibility: Bison-generated code on modern GCC needs <cstdint>
PARSER_CFLAGS := -std=c++17 -O3 -fPIC -include cstdint

# Explicit source lists — every .cpp file in the project is listed here
# so the build graph is clear and no future source is silently pulled
# into an unintended archive.
MAIN_SRCS     := src/main.cpp
STORAGE_SRCS  := src/Domains.cpp src/Files.cpp src/StorageEngine.cpp src/Tables.cpp
INTERP_SRCS   := src/SQLInterface.cpp src/SQLInterpreter.cpp
C_SRCS        := libs/linenoise/linenoise.c

# ======================================================================
# Test source lists — every .cpp file under tests/ is listed here so
# the build graph is clear and compdb captures every compile command.
# ======================================================================
#
# Green (passing) core regression suite — TEST-2 owned files.
TEST_MAIN_SRC       := tests/TestMain.cpp
GREEN_TEST_SRCS     := tests/DomainsTests.cpp tests/RecordsTests.cpp \
                       tests/HeapFileTests.cpp tests/TablesTests.cpp

# Future (intentionally red) SQL specification suite — TEST-3 owned.
FUTURE_MAIN_SRC     := tests/FutureTestMain.cpp
FUTURE_TEST_SRCS    := tests/future/SqlWorkflowTests.cpp

# Test compilation uses the same flags and include paths as production code,
# plus the test harness header directory.
TEST_INCLUDES   := $(INCLUDES) -Itests

# Test binaries live under build/tests/ so $ORIGIN must go up one level
# to find libsqlparser.so in build/.
TEST_LDFLAGS    := -Lbuild -Wl,-rpath,'$$ORIGIN/..'

MAIN_OBJS     := $(patsubst src/%.cpp,build/obj/%.o,$(MAIN_SRCS))
STORAGE_OBJS  := $(patsubst src/%.cpp,build/obj/%.o,$(STORAGE_SRCS))
INTERP_OBJS   := $(patsubst src/%.cpp,build/obj/%.o,$(INTERP_SRCS))
C_OBJS        := build/obj/linenoise.o

ALL_OBJS      := $(MAIN_OBJS) $(STORAGE_OBJS) $(INTERP_OBJS) $(C_OBJS)
DEPS          := $(ALL_OBJS:.o=.d)

# Archives group project objects; main.o and linenoise.o link directly.
# This mirrors the original CMake library layout and keeps each library's
# role — storage vs. interpreter — explicit in the build graph.
LIB_STORAGE     := build/libStorageEngine.a
LIB_STORAGE_OBJ := $(STORAGE_OBJS)
LIB_INTERP      := build/libSQLInterpreter.a
LIB_INTERP_OBJ  := $(INTERP_OBJS)

# ======================================================================
# Test object and binary paths
# ======================================================================
TEST_MAIN_OBJS    := $(patsubst tests/%.cpp,build/tests/%.o,$(TEST_MAIN_SRC))
GREEN_TEST_OBJS   := $(patsubst tests/%.cpp,build/tests/%.o,$(GREEN_TEST_SRCS))
FUTURE_MAIN_OBJS  := $(patsubst tests/%.cpp,build/tests/%.o,$(FUTURE_MAIN_SRC))
FUTURE_TEST_OBJS  := $(patsubst tests/%.cpp,build/tests/%.o,$(FUTURE_TEST_SRCS))

GREEN_TEST_TARGET  := build/tests/test-green
FUTURE_TEST_TARGET := build/tests/test-future

TARGET := build/MiniDBMS

# ======================================================================
# Default target
# ======================================================================

all: $(TARGET)

# Executable: main.o and linenoise.o are linked directly; the static
# archives supply the remaining project code.  Archive groups let the
# linker search each archive repeatedly until no new symbols are resolved.
$(TARGET): $(MAIN_OBJS) $(C_OBJS) $(LIB_STORAGE) $(LIB_INTERP) $(BUILD_PARSER)
	$(CXX) $(CXXFLAGS) $(LDFLAGS) -o $@ $(MAIN_OBJS) $(C_OBJS) \
	  -Wl,--start-group $(LIB_STORAGE) $(LIB_INTERP) -Wl,--end-group $(LDLIBS)

# ======================================================================
# Archives
# ======================================================================

$(LIB_STORAGE): $(LIB_STORAGE_OBJ)
	@mkdir -p $(@D)
	$(AR) rcs $@ $^

$(LIB_INTERP): $(LIB_INTERP_OBJ)
	@mkdir -p $(@D)
	$(AR) rcs $@ $^

# ======================================================================
# Parser library
# ======================================================================

$(PARSER_LIB):
	$(MAKE) -C $(PARSER_DIR) LIB_CFLAGS="$(PARSER_CFLAGS)"

$(BUILD_PARSER): $(PARSER_LIB)
	@mkdir -p $(@D)
	cp $< $@

# ======================================================================
# C++ compilation (project sources)
# ======================================================================

build/obj/%.o: src/%.cpp
	@mkdir -p $(@D)
	$(CXX) $(CXXFLAGS) $(INCLUDES) -c -o $@ $<

# ======================================================================
# C compilation (linenoise)
# ======================================================================

build/obj/linenoise.o: libs/linenoise/linenoise.c
	@mkdir -p $(@D)
	$(CC) $(CFLAGS) $(INCLUDES) -c -o $@ $<

# ======================================================================
# Test compilation
# ======================================================================

# Pattern rule: any .cpp under tests/ compiles to build/tests/.
build/tests/%.o: tests/%.cpp tests/TestHarness.hpp
	@mkdir -p $(@D)
	$(CXX) $(CXXFLAGS) $(TEST_INCLUDES) -c -o $@ $<

# ======================================================================
# Test binaries
# ======================================================================

# Green test runner: links harness main + green test objects against
# project archives and the parser library.
$(GREEN_TEST_TARGET): $(TEST_MAIN_OBJS) $(GREEN_TEST_OBJS) \
                      $(LIB_STORAGE) $(LIB_INTERP) $(BUILD_PARSER)
	@mkdir -p $(@D)
	$(CXX) $(CXXFLAGS) $(TEST_LDFLAGS) -o $@ $(TEST_MAIN_OBJS) $(GREEN_TEST_OBJS) \
	  -Wl,--start-group $(LIB_STORAGE) $(LIB_INTERP) -Wl,--end-group $(LDLIBS)

# Future test runner: dedicated binary for the (currently red) SQL
# specification suite.
$(FUTURE_TEST_TARGET): $(FUTURE_MAIN_OBJS) $(FUTURE_TEST_OBJS) \
                       $(LIB_STORAGE) $(LIB_INTERP) $(BUILD_PARSER)
	@mkdir -p $(@D)
	$(CXX) $(CXXFLAGS) $(TEST_LDFLAGS) -o $@ $(FUTURE_MAIN_OBJS) $(FUTURE_TEST_OBJS) \
	  -Wl,--start-group $(LIB_STORAGE) $(LIB_INTERP) -Wl,--end-group $(LDLIBS)

# ======================================================================
# Test execution targets
# ======================================================================

.PHONY: test test-future test-objs test-clean

test: $(GREEN_TEST_TARGET)
	./$(GREEN_TEST_TARGET)

test-future: $(FUTURE_TEST_TARGET)
	./$(FUTURE_TEST_TARGET)

# Build-only target that compiles all test objects without linking or
# running.  Used by `make compdb` to capture compile commands for LSP.
test-objs: $(TEST_MAIN_OBJS) $(GREEN_TEST_OBJS) $(FUTURE_MAIN_OBJS) $(FUTURE_TEST_OBJS)

# Remove test build artifacts and generated fixture data (preserves
# production build).
test-clean:
	rm -rf build/tests build/test-data

# ======================================================================
# Test dependency files
# ======================================================================

TEST_DEPS := $(TEST_MAIN_OBJS:.o=.d) $(GREEN_TEST_OBJS:.o=.d) \
             $(FUTURE_MAIN_OBJS:.o=.d) $(FUTURE_TEST_OBJS:.o=.d)

# ======================================================================
# Dependency file auto-inclusion
# ======================================================================

-include $(DEPS) $(TEST_DEPS)

# ======================================================================
# Housekeeping
# ======================================================================

.PHONY: all clean compdb

clean:
	rm -rf build/obj build/MiniDBMS build/libStorageEngine.a \
	  build/libSQLInterpreter.a build/libsqlparser.so build/tests \
	  build/test-data

# Rebuild from scratch under Bear so that compile_commands.json captures
# every compile command for clangd / LSP tooling.  Also builds all test
# objects (without linking or running) so that LSP sees the test sources.
compdb:
	rm -f compile_commands.json
	$(MAKE) clean
	bear -- $(MAKE) all test-objs
