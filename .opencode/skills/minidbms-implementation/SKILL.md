---
name: minidbms-implementation
description: Implement an architect-assigned miniDBMS subtask in readable C++17 with scoped edits, safe commits, and focused build verification
compatibility: opencode
metadata:
  project: miniDBMS
  role: implementer
---

# miniDBMS implementation

Use this skill only after the architect supplies a readable plan under `docs/` and identifies your subtask.

## Assignment contract

- Read the full plan before editing.
- Treat the plan's design, dependencies, owned files, and acceptance criteria as binding.
- Do not make design decisions, expand scope, edit another implementer's owned files, or opportunistically refactor adjacent code.
- If a needed choice is absent, a dependency is incomplete, or a repository invariant conflicts with the plan, stop and report the exact blocker to the architect.
- Preserve unrelated user changes in the shared worktree.

## Implementation style

- Write straightforward C++17 that makes the underlying DBMS idea visible.
- Prefer small functions, explicit state, descriptive names, and comments that explain database semantics rather than restating syntax.
- Avoid new dependencies and large abstraction layers unless the plan explicitly calls for them.
- Maintain the fixed-width raw-record representation.
- Preserve the `Relation` layout of all key fields followed by all non-key fields.
- Remember that `HeapFile` deletion replaces a removed record with the final record; never assume stable record order.
- In `src/Tables.cpp`, include `StorageEngine.hpp` before `Tables.hpp` because of the current header cycle.
- Preserve the parser's forced `<cstdint>` compatibility flag unless the plan upgrades the parser.

## Build and verification

- If a pinned submodule directory (`libs/sql-parser/` or `libs/linenoise/`) is empty, report that `git submodule update --init --recursive` is required before building.
- Use `make` for incremental builds; the default target produces `build/MiniDBMS`.
- Prefer the relevant focused build while iterating:
  - `make build/libStorageEngine.a`
  - `make build/libSQLInterpreter.a`
- Finish assigned integration work with `make` when requested.
- The repository has no project tests, lint task, formatter task, typecheck, or CI. Do not claim otherwise and do not substitute parser-submodule tests.
- Expected `std::iterator` deprecation warnings are not new failures.

## Commit discipline

Commit only on explicit architect instruction. Before committing, inspect `git status` and the exact diff, stage only assigned paths, and never include another agent's or the user's changes. Never push. Report the commit hash and validation result to the architect.
