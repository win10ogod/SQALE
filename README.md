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

- This repo contains a complete minimal vertical slice: parse → typecheck → interpret. 
- LLVM backend stubs emit textual IR without linking to LLVM when `USE_LLVM=0`. 
- With LLVM installed, you can build a native `sqale` compiler that emits and JITs via LLVM C API.

Design Highlights

- Language name: SQALE (Square Lisp Engine). File extension: `.sq`.
- Core forms: `def`, `let`, `fn`, `if`, `do`, calls, `spawn`, `chan`, `send`, `recv`, `quote`.
- Types: `Int`, `Float`, `Bool`, `Str`, `Unit`, function types `(T1 ... -> R)`.
- Static typing with local inference for literals; explicit function signatures recommended.
- Concurrency: M:N not required; basic threads over OS threads + bounded channels.
- Memory safety: GC, checked vector ops (future), never expose raw pointers to user code.

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
  [fn [[k : Int]] : (Int -> Int)
    [fn [[x : Int]] : Int [+ x k]]]]

[def main : [ -> Int]
  [fn [] : Int
    [let [[plus2 : (Int -> Int)] [make-adder 2]]
      [print [plus2 40]]
      0]]]
```

3) Threads and channels

```
[def worker : [Int [Chan Int] -> Unit]
  [fn [[n : Int] [out : (Chan Int)]] : Unit
    [send out [+ n 1]]]]

[def main : [ -> Int]
  [fn [] : Int
    [let [[c : (Chan Int)] [chan : Int]]
      [spawn [fn [] : Unit [worker 41 c]]]
      [print [recv c]]
      0]]]
```

REPL

- Build and run `./build/sqale repl` then enter forms interactively.
- All code is typechecked before execution in the interpreter.

Build

Requirements (interpreter only):
- C11 compiler (clang or gcc). No external dependencies.

Optional (LLVM backend):
- LLVM 14+ with C API headers and libs. Set `USE_LLVM=1` and tweak `LLVM_CONFIG` if needed.

Commands

```
make              # builds interpreter-only binary into build/sqale
make USE_LLVM=1   # builds with LLVM when installed
```

CLI

```
./build/sqale repl                    # interactive REPL
./build/sqale run examples/hello.sq   # parse, typecheck, interpret
./build/sqale emit-ir examples/hello.sq -o out.ll
```

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
  README.md
  Makefile
  include/
    sqale.h              # CLI and global context
    arena.h              # small arena allocator for temps
    str.h                # safe string builder
    vec.h                # dynamic array helper (for compiler internals)
    token.h              # lexer tokens
    ast.h                # parsed nodes
    type.h               # types + typechecker
    env.h                # environments (types + values)
    value.h              # runtime values
    gc.h                 # GC interface
    runtime.h            # runtime/stdlib API
    thread.h             # cross-platform threads/channels
    parser.h             # parse S-expr with []
    eval.h               # interpreter
    codegen.h            # LLVM IR backend (optional)
  src/
    main.c
    arena.c str.c vec.c
    lexer.c parser.c
    ast.c type.c env.c
    gc.c value.c runtime.c
    thread.c channel.c
    eval.c codegen_llvm.c
    repl.c
  examples/
    hello.sq threads.sq functions.sq
  tests/
    smoke.sq
```

License

- This is a teaching/sample implementation intended to be small and clear.
