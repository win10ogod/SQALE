SQALE — A small, statically typed, homoiconic, bracket‑Lisp with LLVM

Overview

- Square‑bracket syntax: `[ ... ]` S‑expressions to avoid paren hell.
- Statically typed, functional first: `fn`, first‑class functions, closures.
- Homoiconic: code is data; quote/quasiquote, macro expansion pass (minimal example included).
- Own parser, AST, type checker, interpreter, and LLVM backend (optional).
- Cross‑platform runtime (Windows, Linux, macOS) with threads and channels.
- Memory safety via a compact precise, stop‑the‑world mark & sweep GC.
- REPL and simple package‑less stdlib implemented in the runtime (I/O, math, concurrency).

Status

- Complete vertical slice: parse → macro‑expand → typecheck → interpret.
- LLVM backend emits textual IR; runnable `main` with print shims. Use `clang` to compile IR to native.
- Auto `main` execution for `run`: finds and calls zero‑arg `main : [ -> Int ]`.

Design Highlights

- Language name: SQALE (Square Lisp Engine). File extension: `.sq`.
- Core forms: `def`, `let`, `fn`, `if`, `do`, calls, `spawn`, `chan`, `send`, `recv`, `quote`, `quasiquote`.
- Types: `Int`, `Float`, `Bool`, `Str`, `Unit`, function types `[T1 ... -> R]`, channels `[Chan T]`.
- Collections (v1): `[Vec Any]` with `vec/vec-push/vec-get/vec-len`, minimal `(Map Str Int)` with `map/map-set/map-get/map-len`.
- Functional first: first‑class functions/closures, lexical scoping. Homoiconic with AST values.
- Concurrency: OS threads + bounded channels.
- Memory safety: precise GC; no raw pointer exposure to user code.

Examples

1) Hello world

```
[def main : [ -> Int]
  [fn [] : Int
    [print "Hello, SQALE!"]
    0]]
```

2) Functions and closure

```
[def add : [Int Int -> Int] [fn [[a : Int] [b : Int]] : Int [+ a b]]]
[def make-adder : [Int -> [Int -> Int]]
  [fn [[k : Int]] : [Int -> Int]
    [fn [[x : Int]] : Int [+ x k]]]]

[def main : [ -> Int]
  [fn [] : Int
    [let [[plus2 : [Int -> Int] [make-adder 2]]]
      [print [plus2 40]]
      0]]]
```

3) Threads and channels

```
[def worker : [Int [Chan Int] -> Unit]
  [fn [[n : Int] [out : [Chan Int]]] : Unit
    [send out [+ n 1]]]]

[def main : [ -> Int]
  [fn [] : Int
    [let [[c : [Chan Int]] [chan]]
      [spawn [fn [] : Unit [worker 41 c]]]
      [print [recv c]]
      0]]]
```

Macros & REPL

- defmacro with `quasiquote`, `unquote`, `unquote-splicing`; built‑ins: `when`, `cond`, `->`.
- Macro helpers (runtime): `list?`, `list-len`, `list-head`, `list-tail`, `list-cons`, `list-append`, `symbol?`, `symbol=`.
- `./build/sqale repl` for interactive development; all forms typechecked before execution.

Build

Requirements (interpreter only):
- C11 compiler (clang or gcc). No external dependencies.

Optional (LLVM backend):
- LLVM 14+ with C API headers and libs. Set `USE_LLVM=1` and tweak `LLVM_CONFIG` if needed.

Commands

```
make                 # build interpreter into build/sqale
make USE_LLVM=1      # build with LLVM enabled
./build/sqale repl   # REPL
./build/sqale run examples/hello.sq
./build/sqale emit-ir examples/hello.sq -o out.ll
clang out.ll -O2 -o a.out  # compile IR to native
```

Imports / Packages

- Import search roots: `./`, `packages/`, `std/`, `sqale/packages/`, `sqale/std/`, plus `SQALE_PATH`.
- Official sample: `sqale/packages/official/hello.sq`.

Security & Memory Safety

- No unsafe casts between unrelated representations; all dynamic allocations tracked by GC.
- Bounds checks on strings; future work: safe vectors/maps with checked ops.
- Avoids dangerous libc APIs; stdlib functions are provided by the runtime abstraction.
- Cross-platform threading with safe channel abstractions (no shared mutable data by default).

White House Secure Software Development alignment (high-level)

- Reproducible builds via `Makefile`, pinned compile flags, warnings-as-errors in CI.
- Threat surface minimized: no dynamic `dlopen`, no eval of untrusted text beyond parser.
- Memory safety: GC, checked operations, no raw pointer exposure to end-user code.

Project layout

```
sqale/
  README.md, Makefile
  include/      # headers (AST, types, env, runtime, GC, codegen, macros)
  src/          # lexer, parser, macro, type, eval, runtime, threads, codegen, CLI
  std/          # standard macro library (initial)
  packages/     # third‑party packages (official samples under packages/official)
  examples/     # hello, functions, threads, macros/, collections, wordcount
  tests/        # smoke tests
  docs/         # design notes
```

Documentation Quick Links

- Repository Guidelines: ../AGENTS.md
- Contributing Guide: ../CONTRIBUTING.md
- Code of Conduct: ../CODE_OF_CONDUCT.md
- Roadmap: ../ROADMAP.md
- Design Notes: docs/DESIGN.md
- PR Template: ../.github/PULL_REQUEST_TEMPLATE.md

Contributing

- See `AGENTS.md` for coding style, commit/PR guidance, and developer workflow.
- Style: C11, `-Wall -Wextra -Werror`, 2‑space indent, snake_case for functions, PascalCase for types.
- Validate with small `.sq` examples; include new examples/tests with features.
- Optional sanitizers: `CFLAGS+=" -fsanitize=address,undefined -fno-omit-frame-pointer"`.

Roadmap

- See `ROADMAP.md` for the full, living roadmap (past → present → future).
- Near‑term items:
  - Macro stdlib: move `when/cond/->/->>/let*` to library macros using AST helpers.
  - LLVM lowering v1: lower `let`/`if`/`+`/`-`/`print` (main + functions), link runtime shims by default.
  - Generics v1: type variables + unification; `Vec[T]`, `Map[K,V]`; acceptance with quicksort/wordcount.
  - Modules & packages: dotted imports, `package.toml`, deterministic builds, minimal test runner.
  - GC tracing & profiling: trace closures/envs/containers, incremental mode, heap stats, leak tracker.
  - Tooling: formatter/linter, language server, improved Windows packaging.

License

- This is a teaching/sample implementation intended to be small and clear.
