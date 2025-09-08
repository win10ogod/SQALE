# Repository Guidelines

## Project Structure & Module Organization
- `sqale/` root:
  - `include/` C headers (AST, types, runtime, GC, codegen).
  - `src/` C sources (lexer, parser, typechecker, eval/VM, runtime, threads, LLVM backend, CLI).
  - `examples/` runnable `.sq` programs (hello, functions, threads).
  - `tests/` smoke tests in SQALE.
  - `docs/` design notes.
  - `Makefile` build entry; `scripts/` utility scripts.

## Build, Test, and Development Commands
- Build (interpreter only): `cd sqale && make`
- Build with LLVM backend: `cd sqale && make USE_LLVM=1`
- REPL: `./build/sqale repl`
- Run a file: `./build/sqale run examples/hello.sq`
- Emit LLVM IR: `./build/sqale emit-ir examples/hello.sq -o out.ll`
- Smoke tests: `bash sqale/scripts/run_tests.sh`
- Windows: build via MSYS2/MinGW or clang/LLVM; run the same `make` targets.

## Coding Style & Naming Conventions
- Language: C11, warnings-as-errors (`-Wall -Wextra -Werror`).
- Indentation: 2 spaces; K&R braces; no tabs.
- Files: `snake_case.c/.h` in `src/` and `include/`.
- Functions: `lower_snake_case` (e.g., `eval_program`, `rt_print`).
- Types/Enums: `PascalCase` (e.g., `Type`, `Env`, `ValueKind`).
- Avoid one-letter names; prefer descriptive, short identifiers.

## Testing Guidelines
- Add executable examples to `sqale/examples/` and smoke tests to `sqale/tests/`.
- Prefer small, deterministic programs; verify via `./build/sqale run <file.sq>`.
- For new runtime APIs, include a minimal REPL snippet in PR description.

## Commit & Pull Request Guidelines
- Commits: imperative mood, concise scope. Example: `parser: handle arrow tokens`.
- Group related changes; avoid mixing refactors and features in one commit.
- PRs must include: summary, motivation, testing steps/outputs, and any perf or compatibility notes.
- Link related issues; add screenshots or terminal output for REPL/CLI behavior.

## Security & Configuration Tips
- Favor safe helpers (arena, vec, str) and checked operations.
- Keep additions `-Wall -Wextra -Werror` clean; consider testing with `ASAN` locally (`CFLAGS+=" -fsanitize=address -fno-omit-frame-pointer"`).
# Repository Guidelines

## Project Structure & Module Organization
- `sqale/src/` C sources (lexer, parser, macro expander, typechecker, evaluator/VM, runtime/GC, threads/channels, LLVM codegen, CLI).
- `sqale/include/` public headers shared across modules.
- `sqale/std/` standard macro library; `sqale/packages/official/` sample third‑party packages.
- `sqale/examples/` runnable programs and macro/collection demos; `sqale/tests/` smoke tests.
- `sqale/Makefile` build entry; `sqale/docs/` design notes; `sqale/scripts/` utility scripts.

## Build, Test, and Development Commands
- Build (interpreter): `cd sqale && make`
- Build with LLVM: `make USE_LLVM=1`
- REPL: `./build/sqale repl`
- Run file: `./build/sqale run examples/hello.sq`
- Emit IR: `./build/sqale emit-ir examples/hello.sq -o out.ll`
- Smoke tests: `bash sqale/scripts/run_tests.sh`
- Windows: cross‑compile via MinGW/clang; package: `make package-win`.

## Coding Style & Naming Conventions
- Language: C11; strict flags `-Wall -Wextra -Werror -fno-strict-aliasing`.
- Indentation: 2 spaces, no tabs; K&R braces.
- Files/functions: `snake_case` (e.g., `eval_program`, `rt_print`). Types/enums: `PascalCase` (e.g., `Type`, `ValueKind`).
- Keep functions short; prefer small helpers; avoid global state except VM/runtime singletons.

## Testing Guidelines
- Prefer self‑contained `.sq` examples under `sqale/examples/` (e.g., `macros/*.sq`, `collections.sq`).
- Keep tests deterministic and printable; validate by running `./build/sqale run <file.sq>`.
- For new runtime APIs, include a minimal REPL snippet in PR and an example file when reasonable.

## Commit & Pull Request Guidelines
- Commits: imperative, scoped prefix (e.g., `lexer: handle arrows`, `codegen: add add/sub`).
- PRs must include: purpose, design notes, testing commands/output, risk/compatibility notes, and any performance implications. Link related issues.

## Security & Configuration Tips
- Use sanitizers locally for memory safety: `CFLAGS+=" -fsanitize=address,undefined -fno-omit-frame-pointer"`.
- Import search paths configurable via `SQALE_PATH` (colon‑separated). Avoid untrusted modules by default.

## Architecture Overview
- Pipeline: `lexer → parser → macro expand → typecheck → (eval | LLVM codegen)`.
- Runtime: precise GC, channels/threads, stdlib (I/O, math, collections), macro helpers for code‑as‑data.
- LLVM backend: emits textual IR; optional real LLVM C API when `USE_LLVM=1`.

## Agent‑Specific Instructions
- Use `apply_patch` for edits; keep diffs focused and style‑consistent.
- Do not commit or reformat unrelated files; prefer small, incremental changes with clear rationale.
