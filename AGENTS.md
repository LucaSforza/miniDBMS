# Repository Guide

## Setup and Build

- Initialize the pinned Hyrise parser before configuring: `git submodule update --init --recursive`. `libs/sql-parser/` is otherwise empty.
- The build requires CMake 3.10+, a C++17 compiler, `make`, and Curses development headers/libraries.
- Configure and build with `cmake -S . -B build` then `cmake --build build`. CMake invokes `make` inside `libs/sql-parser/`, links its `libsqlparser.so`, and copies that library beside `build/MiniDBMS`.
- The parser build deliberately forces `<cstdint>` through `LIB_CFLAGS`; the pinned parser's generated Bison code otherwise fails on modern GCC because global `uintmax_t` is undeclared. Preserve this compatibility flag unless the submodule is upgraded.
- For focused compile checks, use `cmake --build build --target StorageEngine` or `cmake --build build --target SQLInterpreter`.
- Generate API documentation with `doxygen Doxyfile`; output goes under ignored `docs/`.

## Current Baseline

- There are no project tests, lint/format/typecheck tasks, CI workflows, or pre-commit hooks. Do not mistake the parser submodule's tests for this project's test suite.
- A clean full build succeeds on modern GCC but emits expected deprecation warnings because the file API still uses C++17's deprecated `std::iterator`.

## Architecture and Data Rules

- Runtime flow is `src/main.cpp` -> `SQLInterface` (stdin REPL; `exit`/`quit`) -> `SQLInterpreter` -> the Hyrise SQL parser. The interpreter currently dispatches only `SELECT`, and its execution path is mostly a stub; the default REPL constructs it without a `Database`.
- Storage is split across domains (`Domains.*`), records/schema/database (`StorageEngine.*`), binary files (`Files.cpp`, `File.hpp`, `HeapFile.hpp`), and table abstractions (`Tables.*`).
- `StorageEngine.hpp` and `Tables.hpp` are cyclically coupled. `src/Tables.cpp` must include `StorageEngine.hpp` first; including `Tables.hpp` directly leaves `PhysicalTable` unavailable when `Database` is declared.
- Records are fixed-width raw byte strings. A `Relation` lays out all key fields first, followed by non-key fields; `HeapFile` key lookup assumes that prefix layout. Preserve exact domain sizes and field order when changing serialization.
- `HeapFile` is an in-place binary heap file. Deletion replaces the removed record with the final record and truncates on destruction; it does not preserve record order.
- Runtime database data belongs under ignored `databases/`; build output is under ignored `build/`.
