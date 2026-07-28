# miniDBMS

miniDBMS is a deliberately small C++17 database management system designed for
learning database theory through readable, straightforward code. It exposes
storage layout, schema management, query dispatch, and execution concepts
without the complexity of a production system.

**Current status** — early development. The REPL dispatches only `SELECT`
statements, and its execution path is mostly a stub. There is no SQL data
manipulation language, no test suite, and no stability guarantee. The project
exists to make internal DBMS concepts visible.

---

## Prerequisites

- GNU Make
- GCC (or a compatible C++17 compiler) and `g++`/`gcc`
- [Bear](https://github.com/rizsotto/Bear) (optional, for LSP support)
- Curses and CMake are **not** required

---

## Setup

Clone the repository and initialize both pinned Git submodules:

```sh
git submodule update --init --recursive
```

This populates `libs/sql-parser/` (Hyrise SQL parser) and `libs/linenoise/`
(line-editing library used by the REPL).

---

## Build

```sh
make
```

Incremental builds are supported — only changed sources are recompiled.

### Output layout

```
build/
  MiniDBMS              — the executable
  libsqlparser.so       — Hyrise SQL parser (resolved at runtime via $ORIGIN)
  libStorageEngine.a    — storage domain archive
  libSQLInterpreter.a   — interpreter archive
  obj/                  — compiled object files and dependency files (*.d)
```

To rebuild only a subsystem:

```sh
make build/libStorageEngine.a
make build/libSQLInterpreter.a
```

### Clean

```sh
make clean
```

---

## Run

```sh
./build/MiniDBMS
```

The REPL uses [linenoise](https://github.com/antirez/linenoise) for one-line
input:

- Non-empty commands are stored in an in-memory history (accessible with
  Up/Down arrow keys).
- Exit with **Ctrl+D** (EOF) or by typing `exit` or `quit`.
- The default REPL constructs the interpreter without a `Database` — most
  queries will report an error or produce no result.

---

## Bear / LSP support (optional)

If Bear is installed, generate a `compile_commands.json` for clangd, Neovim,
or other LSP tools:

```sh
make compdb
```

This runs a clean rebuild under Bear and writes the compilation database to
the repository root (`compile_commands.json` is git-ignored).

### Storage model

- Records are fixed-width raw byte strings.
- A `Relation` lays out all key fields first, followed by non-key fields.
- `HeapFile` is an in-place binary heap file; deletion replaces the removed
  record with the final record and does not preserve record order.
- Runtime database data lives under the ignored `databases/` directory.

---

## Known limitations

- Only `SELECT` statements are dispatched; `INSERT`, `UPDATE`, `DELETE`,
  `CREATE`, and all DDL are not handled.
- Query execution is mostly a stub — the default REPL builds the interpreter
  without a `Database`, so most queries produce an error or do nothing.
- There are **no project-level tests**, linting, formatting, or type-checking
  tasks. The parser submodule has its own test suite; do not mistake it for
  a project test suite.
- `StorageEngine.hpp` and `Tables.hpp` are cyclically coupled —
  `src/Tables.cpp` must include `StorageEngine.hpp` before `Tables.hpp`.
- The build emits deprecation warnings from the use of C++17's deprecated
  `std::iterator` in the file API — these are expected.
- No SQL standard compliance, concurrency, durability guarantees, or
  production deployment support is implied.
