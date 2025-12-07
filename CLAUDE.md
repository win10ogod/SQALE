# CLAUDE.md — AI Assistant Guide for SQALE

This file provides AI assistants with essential context for working effectively with the SQALE codebase. For detailed contributing guidelines, see `AGENTS.md` and `CONTRIBUTING.md`.

## Project Overview

**SQALE** (Square Lisp Engine) is a statically-typed, homoiconic, functional-first programming language with square-bracket S-expression syntax. It's a complete language implementation (~2,500 lines of C) featuring:

- Own lexer, parser, type checker, interpreter, and optional LLVM backend
- Precise mark-and-sweep garbage collector
- OS threads and bounded channels for concurrency
- Macro system with quasiquote/unquote
- Cross-platform support (Linux, macOS, Windows)

**File extension:** `.sq`

## Quick Reference

### Build Commands
```bash
# Build interpreter
make

# Build with LLVM backend
make USE_LLVM=1

# Run REPL
./build/sqale repl

# Execute a program
./build/sqale run examples/hello.sq

# Emit LLVM IR
./build/sqale emit-ir examples/hello.sq -o out.ll

# Compile IR to native (requires runtime)
cc -c src/runtime_llvm.c -o build/runtime_llvm.o
clang out.ll build/runtime_llvm.o -O2 -o a.out

# Run smoke tests
bash scripts/run_tests.sh

# Clean build
make clean
```

### Development with Sanitizers
```bash
make CFLAGS="-std=c11 -O0 -g -Wall -Wextra -Werror -fsanitize=address,undefined -fno-omit-frame-pointer -Iinclude"
```

## Directory Structure

```
SQALE/
├── include/          # C headers (public API)
│   ├── sqale.h       # Main entry point
│   ├── ast.h         # AST node types (NodeKind: N_LIST, N_SYMBOL, etc.)
│   ├── type.h        # Type system (Int, Float, Bool, Str, Func, Chan, etc.)
│   ├── value.h       # Runtime values (ValueKind: VAL_INT, VAL_CLOSURE, etc.)
│   ├── parser.h      # Parser interface
│   ├── eval.h        # Evaluator/VM interface
│   ├── env.h         # Environment/scope management
│   ├── gc.h          # Garbage collector
│   ├── runtime.h     # Runtime system (stdlib)
│   ├── thread.h      # Thread abstraction
│   ├── codegen.h     # LLVM backend
│   ├── macro.h       # Macro system
│   ├── arena.h       # Arena allocator
│   └── ...
│
├── src/              # Implementation files
│   ├── main.c        # CLI entry point (repl, run, emit-ir)
│   ├── lexer.c       # Tokenization
│   ├── parser.c      # Square-bracket S-expr parser
│   ├── type.c        # Type checker
│   ├── eval.c        # Tree-walking evaluator
│   ├── gc.c          # Mark-and-sweep GC
│   ├── runtime.c     # Built-in functions
│   ├── macro.c       # Macro expansion
│   ├── codegen_llvm.c # LLVM IR generation
│   ├── thread.c      # OS thread abstraction
│   ├── channel.c     # Bounded channels
│   └── ...
│
├── examples/         # Runnable .sq programs (serve as tests)
├── tests/            # Smoke tests
├── std/              # Standard macro library
├── packages/         # Third-party packages
├── docs/             # Design notes
└── scripts/          # Utility scripts
```

## Architecture & Pipeline

```
Source (.sq) → [Lexer] → Tokens → [Parser] → AST → [Macro Expander] →
Expanded AST → [Type Checker] → Typed AST → [Evaluator | LLVM Codegen] → Result
```

### Key Components

| Component | Entry Point | Purpose |
|-----------|-------------|---------|
| Lexer | `lexer.c` | Tokenizes `.sq` source |
| Parser | `parser.c` | Builds square-bracket AST |
| Macro System | `macro.c` | Expands macros before type checking |
| Type Checker | `type.c` | Static type inference and checking |
| Evaluator | `eval.c` | Tree-walking interpreter |
| Codegen | `codegen_llvm.c` | LLVM IR emission |
| GC | `gc.c` | Precise mark-and-sweep collector |
| Runtime | `runtime.c` | Built-in functions (print, math, I/O) |

## SQALE Language Syntax

### Basic Forms
```lisp
; Definition
[def name : Type value]

; Function
[fn [[arg1 : T1] [arg2 : T2]] : ReturnType body]

; Let binding
[let [[name : Type value]] body]

; Conditional
[if condition then-expr else-expr]

; Sequence
[do expr1 expr2 ... result]

; Function call
[func arg1 arg2 ...]
```

### Types
- Primitives: `Int`, `Float`, `Bool`, `Str`, `Unit`
- Functions: `[T1 T2 -> R]`
- Channels: `[Chan T]`
- Collections: `[Vec Any]`, `(Map Str Int)` (prototype)

### Example Program
```lisp
[def add : [Int Int -> Int]
  [fn [[a : Int] [b : Int]] : Int
    [+ a b]]]

[def main : [ -> Int]
  [fn [] : Int
    [print [add 2 3]]
    0]]
```

## Coding Conventions

### C Style
- **Language:** C11 with `-Wall -Wextra -Werror -fno-strict-aliasing`
- **Indentation:** 2 spaces, no tabs
- **Braces:** K&R style
- **Naming:**
  - Functions: `snake_case` (e.g., `eval_program`, `rt_print`)
  - Types/Enums: `PascalCase` (e.g., `Type`, `ValueKind`, `NodeKind`)
  - Files: `snake_case.c/.h`

### Common Patterns

**AST Node Creation:**
```c
Node *node = node_new_list(arena, 4);
node_list_push(arena, node, child);
```

**Value Construction:**
```c
Value v = v_int(42);
Value v = v_str(string_ptr);
Value v = v_closure(closure_ptr);
```

**Arena Allocation:**
```c
Arena arena;
arena_init(&arena, 1 << 20);  // 1MB
// ... use arena ...
arena_free(&arena);
```

**Environment Lookup:**
```c
EnvEntry *e = env_lookup(env, "name");
if (e && e->value) { /* use value */ }
```

## Common Tasks

### Adding a New Built-in Function

1. Declare in `include/runtime.h`
2. Implement in `src/runtime.c` following pattern:
   ```c
   static Value rt_my_func(Env *env, Value *args, int nargs) {
     // Validate args
     // Perform operation
     return v_int(result);
   }
   ```
3. Register in `runtime_init()` or equivalent
4. Add example to `examples/`

### Adding a New AST Node Type

1. Add to `NodeKind` enum in `include/ast.h`
2. Add union variant in `struct Node`
3. Add constructor in `src/ast.c`
4. Handle in parser (`src/parser.c`)
5. Handle in type checker (`src/type.c`)
6. Handle in evaluator (`src/eval.c`)
7. Handle in codegen if needed (`src/codegen_llvm.c`)

### Adding a New Value Type

1. Add to `ValueKind` enum in `include/value.h`
2. Add union variant in `struct Value`
3. Add constructor function (e.g., `v_mytype()`)
4. Update GC if heap-allocated (`src/gc.c`)
5. Handle in evaluator and relevant runtime functions

## Testing

### Verification Workflow
1. **Build:** `make` (should compile without warnings)
2. **REPL test:** `./build/sqale repl` then try expressions
3. **Run examples:** `./build/sqale run examples/<file>.sq`
4. **Smoke tests:** `bash scripts/run_tests.sh`

### Adding Tests
- Place runnable `.sq` programs in `examples/`
- Programs should have a `main : [ -> Int]` function
- Return 0 for success, non-zero for failure
- Output should be deterministic and verifiable

## LLVM Backend

The LLVM backend (`src/codegen_llvm.c`) generates LLVM IR from the typed AST.

### Supported Features
- All basic types: Int (i64), Float (double), Bool (i1), Str (i8*), Unit (void)
- Arithmetic: `+`, `-`, `*`, `/`, `%`
- Comparisons: `=`, `<`, `>`, `<=`, `>=`
- Control flow: `if`/`else` with phi nodes
- Variable bindings: `let`
- User-defined functions with recursion
- Sequence blocks: `do`

### Compilation Workflow
```bash
# 1. Generate IR
./build/sqale emit-ir program.sq -o program.ll

# 2. Compile runtime
cc -c src/runtime_llvm.c -o build/runtime_llvm.o

# 3. Link and compile
clang program.ll build/runtime_llvm.o -O2 -o program

# 4. Run
./program
```

### Runtime Functions (runtime_llvm.c)
The generated IR calls these runtime functions:
- `sq_print_i64`, `sq_print_f64`, `sq_print_bool`, `sq_print_cstr` - Output
- `sq_print_newline` - Line termination
- `sq_alloc` - Memory allocation
- `sq_alloc_closure`, `sq_closure_get_fn`, `sq_closure_get_env` - Closure support

### Not Yet Implemented
- Closures (environment capture)
- Higher-order functions passed as values
- Collections (Vec, Map)
- Channels and threading

## Gotchas & Important Notes

1. **Arena vs GC:** AST nodes use arena allocation (bulk freed). Runtime values (closures, strings, channels) use GC.

2. **Type annotations required:** SQALE is statically typed; all function parameters and return types need explicit annotations.

3. **Square brackets everywhere:** Unlike traditional Lisps, SQALE uses `[...]` instead of `(...)`.

4. **Macro expansion before type checking:** Macros operate on raw AST before types are inferred.

5. **No external C dependencies:** Interpreter-only build has zero external deps. LLVM is optional.

6. **Cross-platform threading:** `thread.c` abstracts pthreads (Unix) vs Win32 threads.

7. **Import search paths:** `./` → `packages/` → `std/` → `SQALE_PATH` environment variable.

## Roadmap Context

**Completed (M0-M8):** Core language, runtime, GC, threads/channels, LLVM backend v2 (functions, control flow, recursion), macros, imports, collections v1, Windows support.

**In Progress:** Closures in LLVM backend, macro stdlib (in SQALE).

**Planned:** Generics, module system, GC improvements, tooling (formatter, LSP), LLVM optimization passes.

See `ROADMAP.md` for the full checklist.

## Commit Guidelines

- Use imperative mood with scope prefix: `parser: handle arrow tokens`, `codegen: add add/sub`
- Keep changes focused; one logical change per commit
- Reference issues when applicable
- Include test commands/output in PRs

## Security Considerations

- Use safe helpers (`arena`, `vec`, `str`) with bounds checking
- Avoid raw pointer exposure to user code
- Keep GC-managed objects consistent
- Test with AddressSanitizer during development
- No dynamic `dlopen`, no eval of untrusted text beyond parser
