# Repository Guide

## Setup and Build

- Initialize both pinned dependencies with `git submodule update --init --recursive`; this populates `libs/sql-parser/` and `libs/linenoise/`.
- The build requires GNU Make, GCC-compatible C and C++ compilers, and C++17 support. CMake and Curses are not required.
- Build incrementally with `make`. The top-level Makefile builds the parser through its own Makefile, compiles bundled linenoise as C, and produces `build/MiniDBMS` with an adjacent `build/libsqlparser.so` found through an `$ORIGIN` runtime path.
- The parser build deliberately forces `<cstdint>` through `LIB_CFLAGS`; the pinned parser's generated Bison code otherwise fails on modern GCC because global `uintmax_t` is undeclared. Preserve this compatibility flag unless the submodule is upgraded.
- For focused compile checks, use `make build/libStorageEngine.a` or `make build/libSQLInterpreter.a`.
- Run `make compdb` with Bear installed to force a clean captured build and generate ignored root `compile_commands.json` for clangd/Neovim LSP use.
- Generate API documentation with `doxygen Doxyfile`; output goes under ignored `docs/`.
- Keep the user-facing `README.md` setup, build, REPL, and limitation statements synchronized with the verified Makefile and runtime behavior.

## Current Baseline

- There are no project tests, lint/format/typecheck tasks, CI workflows, or pre-commit hooks. Do not mistake the parser submodule's tests for this project's test suite.
- `make clean && make` succeeds on modern GCC but emits expected deprecation warnings because the file API still uses C++17's deprecated `std::iterator`.

## Architecture and Data Rules

- Runtime flow is `src/main.cpp` -> `SQLInterface` (linenoise one-line REPL) -> `SQLInterpreter` -> the Hyrise SQL parser. Non-empty commands enter in-memory history, a null linenoise result ends the loop as EOF, and exact `exit`/`quit` inputs terminate without dispatch. The interpreter currently dispatches only `SELECT`, and its execution path is mostly a stub; the default REPL constructs it without a `Database`.
- Storage is split across domains (`Domains.*`), records/schema/database (`StorageEngine.*`), binary files (`Files.cpp`, `File.hpp`, `HeapFile.hpp`), and table abstractions (`Tables.*`).
- `StorageEngine.hpp` and `Tables.hpp` are cyclically coupled. `src/Tables.cpp` must include `StorageEngine.hpp` first; including `Tables.hpp` directly leaves `PhysicalTable` unavailable when `Database` is declared.
- Records are fixed-width raw byte strings. A `Relation` lays out all key fields first, followed by non-key fields; `HeapFile` key lookup assumes that prefix layout. Preserve exact domain sizes and field order when changing serialization.
- `HeapFile` is an in-place binary heap file. Deletion replaces the removed record with the final record and truncates on destruction; it does not preserve record order.
- Runtime database data belongs under ignored `databases/`; build output is under ignored `build/`.

## Educational Goal

- This is a deliberately small DBMS for learning database theory. Prefer clear, direct implementations that expose storage, schema, query, and execution concepts over production-grade frameworks or premature abstraction.
- Keep code paths easy to follow from SQL input to storage behavior. Comments should explain DBMS semantics and invariants, not restate C++ syntax.

## OpenCode Agent Team

- Project-local OpenCode configuration lives in `opencode.json` and `.opencode/`. The default `architect` agent plans and reviews; only the `implementer` subagent changes production code.
- Every architect-led task starts with a written plan under ignored `docs/plans/`. Delegations must identify the plan path, subtask, owned files, dependencies, validation, and commit policy.
- Parallel implementation is preferred only for independent subtasks with disjoint file ownership or a settled shared interface.
- The architect reviews every implementer result and the final integrated diff. Review corrections are delegated back to an implementer.
- At the end of every completed agentic cycle, the architect directly updates this file with durable, verified commands, architecture facts, or invariants and removes stale guidance. Do not add transient task status or a session diary.
