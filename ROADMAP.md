# SQALE Roadmap (✔ Checklist)

This file uses checkboxes: done [x], pending [ ]. Updated continuously as milestones land.

## Completed (Past → Now)
- [x] M0: Repo bootstrap
  - Project layout, Makefile, headers/sources scaffolding
- [x] M1: Frontend core
  - Lexer, Parser (square‑bracket S‑expr), AST + Arena, token stream
- [x] M2: Static typing + evaluator
  - Types (Int/Float/Bool/Str/Unit/Func/Chan), type checker, Evaluator/VM, REPL
- [x] M3: Runtime, GC, concurrency
  - Precise GC (mark/sweep), Strings/I/O, OS threads + bounded channels (spawn/send/recv)
- [x] M4: LLVM backend (v1)
  - Text IR emission, runnable main, print shims; int‑literal add/sub lowering
- [x] M5: Macros & Homoiconicity
  - defmacro, quasiquote/unquote/unquote‑splicing; macro expansion phase; AST helpers
- [x] M6: Imports / packages (prototype)
  - Import resolver (paths + SQALE_PATH), retained module arenas, official sample package
- [x] M7: Collections v1 (prototype)
  - Vec Any (vec/push/get/len + printing), minimal Map(Str→Int), str‑split‑ws
- [x] M8: Windows packaging (basic)
  - Packaging target and cross‑compile guidance (MinGW/clang)

## In Progress
- [ ] Macro stdlib (in SQALE)
  - when/cond/->/->>/let*/unless using AST helpers and splicing; macro tests
- [ ] LLVM lowering (core subset)
  - let bindings, if (CFG + phi for Int), +/−, print (Int/Str) for main and plain functions; small SSA builder

## Next (Short Term)
- [ ] Generics v1
  - Type variables + unification; Vec[T], Map[K,V]
  - Acceptance: quicksort (Vec[Int]), wordcount (Map[Str,Int])
- [ ] Modules & packages
  - Dotted imports, package.toml, reproducible builds, minimal test runner
- [ ] Runtime polish
  - Safer containers, more stdlib (time, FS), improved errors/traces

## Mid‑Term
- [ ] GC tracing & profiling
  - Trace closures/envs/containers, incremental/compacting modes, heap stats, leak tracker, perf counters
- [ ] LLVM backend (broader)
  - Closures and calls, constants, basic optimizations (inlining, bounds‑check elision), REPL JIT
- [ ] Tooling
  - Formatter/linter, language server, improved Windows release packaging

## Releases & Versioning
- Acceptance gates:
  - Native examples (hello/functions) build and run
  - Macro stdlib works consistently in REPL and file mode
  - Generics demos (quicksort/wordcount) typecheck and run correctly
- Adopt semver once public CLI stabilizes; pre‑1.0 while language evolves

## How to Influence the Roadmap
- Open an issue with motivation, alternatives, and a minimal acceptance example
- For large designs (syntax/types/IR), start a design PR under `docs/` and iterate in stages
