# miniDBMS

miniDBMS is a deliberately small C++17 database management system designed for
learning database theory through readable, straightforward code. It exposes
storage layout, schema management, query dispatch, and execution concepts
without the complexity of a production system.

**Current status** — early development. The REPL dispatches only `SELECT`
statements, and its execution path is mostly a stub. There is no SQL data
manipulation language implementation (see "Future SQL specifications"
under Tests). The project exists to make internal DBMS concepts visible.
The stabilized storage core is protected by a passing regression suite.

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

### Tests

```sh
make test
```

Builds and runs the **green suite** — passing core-regression tests
that protect domain, record, file, heap-file, and table behavior.
Must exit zero.

```sh
make test-future
```

Builds and runs **future SQL specifications** — executable descriptions
of intended `CREATE TABLE`, `INSERT`, `SELECT`, `UPDATE`, and `DELETE`
workflow. This target is expected to exit **non-zero** until the
corresponding SQL execution is implemented. A passing build with red
assertions means the specification is intact; green assertions signal
working user implementation.

Do **not** mistake the parser submodule's test suite
(`libs/sql-parser/`) for these project-owned suites.

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
- File scans and table lookups/scans return records by value.
- Record updates validate the complete candidate before mutating; on failure
  the original record is preserved unchanged.
- `HeapFile` is an in-place binary heap file; deletion replaces the removed
  record with the final record, immediately resizes the physical file, and
  does not preserve record order.
- Runtime database data lives under the ignored `databases/` directory.

---

## Known limitations

- Only `SELECT` statements are dispatched; `INSERT`, `UPDATE`, `DELETE`,
  `CREATE`, and all DDL are not handled.
- Query execution is mostly a stub — the default REPL builds the interpreter
  without a `Database`, so most queries produce an error or do nothing.
- The parser submodule (`libs/sql-parser/`) has its own test suite;
  do not mistake it for the project-owned suites described under Tests.
- There are no linting, formatting, or type-checking tasks.
- `StorageEngine.hpp` and `Tables.hpp` are cyclically coupled —
  `src/Tables.cpp` must include `StorageEngine.hpp` before `Tables.hpp`.
- The build currently produces no project warnings.
- No SQL standard compliance, concurrency, durability guarantees, or
  production deployment support is implied.
