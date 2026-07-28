---
name: minidbms-architecture
description: Plan, decompose, delegate, and review work on this educational C++17 miniDBMS while preserving its storage invariants
compatibility: opencode
metadata:
  project: miniDBMS
  role: architect
---

# miniDBMS architecture and orchestration

Use this skill for every architect-led task in this repository.

## Product intent

miniDBMS exists to help its author learn DBMS theory. Prefer direct, inspectable implementations of database concepts over frameworks, clever abstractions, premature optimization, or production-only machinery. Keep responsibilities and data flow easy to trace in a debugger and in source code.

## Repository map and invariants

- Runtime flow: `src/main.cpp` -> `SQLInterface` -> `SQLInterpreter` -> Hyrise SQL parser.
- Storage domains live in `Domains.*`; records, schemas, and databases in `StorageEngine.*`; binary file behavior in `Files.cpp`, `File.hpp`, and `HeapFile.hpp`; table abstractions in `Tables.*`.
- The interpreter currently dispatches only `SELECT`, most execution is a stub, and the default REPL constructs it without a `Database`.
- `StorageEngine.hpp` and `Tables.hpp` are cyclically coupled. `src/Tables.cpp` must include `StorageEngine.hpp` before `Tables.hpp`.
- Records are fixed-width raw byte strings. A `Relation` serializes every key field first and then every non-key field. `HeapFile` lookup depends on that key-prefix layout.
- `HeapFile` deletion swaps the final record into the removed slot, immediately truncates the physical file, and does not preserve record order.
- Preserve exact domain sizes, field order, on-disk compatibility, and ownership/lifetime assumptions unless the plan explicitly designs a migration.

## Mandatory plan

Create `docs/plans/<task-slug>.md` before invoking an implementer. The plan must state:

1. Goal and the DBMS concept the change should teach.
2. Current behavior and relevant invariants.
3. Chosen design, including deliberately rejected complexity when useful.
4. Subtasks with stable IDs, dependencies, acceptance criteria, and exact file ownership.
5. A parallelization decision. Group independent subtasks explicitly; serialize overlapping files or contracts.
6. Focused and full validation commands.
7. Review checkpoints and commit policy.
8. The durable `AGENTS.md` knowledge expected from the completed cycle.

Pass the exact plan path and one or more explicit subtask IDs in every delegation. Multiple implementers may work concurrently only on disjoint files or on changes with a settled shared interface. Do not delegate competing edits to the same file.

## Review protocol

- Inspect the implementer's complete diff; never rely only on its summary.
- Check the diff against the plan, educational clarity, C++17 constraints, serialization layout, file semantics, and error handling.
- Use a focused build first: `make build/libStorageEngine.a` or `make build/libSQLInterpreter.a` as applicable.
- Use `git submodule update --init --recursive` to fetch both pinned submodules, then `make` for final integration.
- Require `make test` to pass (green core-regression suite; must exit zero).
- `make test-future` is expected to exit non-zero until the user implements the SQL workflows; its red output is a benchmark, not a current failure. Do not report parser-submodule tests as project tests.
- Send review fixes back to an implementer with the plan path and a new or reopened subtask ID.
- At cycle end, directly update `AGENTS.md` with verified, reusable facts. Remove stale guidance when necessary; do not turn it into a task log.
