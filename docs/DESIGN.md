SQALE Language Design Notes

Goals

- Functional-first, statically typed, homoiconic language with square-bracket S-exprs.
- Small but practical: own parser, type checker, interpreter, runtime stdlib, and LLVM backend.
- Concurrency using threads + channels; memory safety with a precise mark & sweep GC.

Syntax (informal)

- Atoms: integer `123`, float `1.23`, boolean `true|false`, string `"..."`, symbol `name`.
- Lists: `[ head arg1 arg2 ... ]` where `head` is a symbol or expression.
- Special forms:
  - `[def name : Type expr]` — toplevel/global definition.
  - `[let [ [name : Type expr] ... ] body ...]` — local bindings; type can be omitted when inferable.
  - `[fn [ [x : T] [y : U] ... ] : R body ...]` — function literal (closure).
  - `[if cond then-expr else-expr]` — conditional.
  - `[do e1 e2 ...]` — sequencing.
  - Concurrency: `[chan]`, `[send ch v]`, `[recv ch]`, `[spawn closure]`.

Types

- Primitives: `Int`, `Float`, `Bool`, `Str`, `Unit`, `Any`.
- Channels: `[Chan T]`.
- Functions: `[T1 T2 -> R]` with 0+ params; zero-arg is `[ -> R ]`.
- `Any` is only used to simplify printing and basic demos; it unifies with any other type.

Homoiconicity & Macros (v1)

- The parser produces an AST of Nodes; `quote` is reserved. The macro system is stubbed in v1.
- v2 will add `quote`, `quasiquote`, and `defmacro` with compile-time evaluation of AST transformers.

Typechecking

- Minimal structural typechecker annotates each Node with a Type and ensures consistency.
- No general unification or generics in v1, but function types and channels are checked.
- Overloads are not implemented; `print` uses `Any` for ergonomic output.

Runtime & Safety

- GC: precise, stop-the-world mark & sweep (v1 marks leaf nodes, adequate for Strings/Closures used now).
- Strings are length-tracked; no raw pointer exposure to user programs.
- Channels are bounded and safe; no shared mutable memory exposed by default.
- Platform abstraction uses pthreads on POSIX and Win32 threads on Windows.

LLVM Backend

- Textual IR emitter is shipped by default; building with `USE_LLVM=1` uses LLVM-C API and emits a minimal `main`.
- v2 will lower typed AST to IR, link the runtime shim, and JIT or emit native executables.

Roadmap

1. Macro system with compile-time evaluation of AST transformers.
2. Richer stdlib: vectors/maps with bounds checks; math; file system; time.
3. Proper module system and imports.
4. Full GC tracing for Closures and future containers.
5. LLVM lowering for a core subset; JIT for the REPL; AOT for `sqale build`.

