# SQALE Language Completeness Roadmap

This document outlines the gap analysis between SQALE and modern systems languages (particularly Zig), and provides a prioritized implementation roadmap.

## Current State Analysis

### SQALE Keywords/Special Forms
| Category | Available | Status |
|----------|-----------|--------|
| Definition | `def`, `defmacro`, `fn`, `let` | ✅ Complete |
| Control | `if`, `do` | ⚠️ Partial |
| Metaprogramming | `quote`, `quasiquote`, `unquote`, `unquote-splicing` | ✅ Complete |
| Modules | `import` | ⚠️ Partial |

### SQALE Built-in Functions
| Category | Functions | Status |
|----------|-----------|--------|
| Arithmetic | `+`, `-`, `*`, `/`, `mod`, `%`, `neg` | ✅ Complete |
| Comparison | `=`, `!=`, `<`, `>`, `<=`, `>=` | ✅ Complete |
| Logical | `not`, `and`, `or` | ✅ Complete |
| I/O | `print` | ⚠️ Basic |
| Strings | `str-concat`, `str-len`, `str-split-ws` | ⚠️ Partial |
| Collections | `vec`, `vec-push`, `vec-get`, `vec-len` | ⚠️ Partial |
| Maps | `map`, `map-set`, `map-get`, `map-len` | ⚠️ Partial |
| Concurrency | `chan`, `send`, `recv`, `spawn` | ✅ Complete |
| List/Macro | `list?`, `symbol?`, `symbol=`, `list-*` | ✅ Complete |

### SQALE Types
| Type | Syntax | Status |
|------|--------|--------|
| Integer | `Int` | ✅ |
| Float | `Float` | ✅ |
| Boolean | `Bool` | ✅ |
| String | `Str` | ✅ |
| Unit | `Unit` | ✅ |
| Function | `[T1 T2 -> R]` | ✅ |
| Channel | `[Chan T]` | ✅ |
| Vector | `[Vec T]` | ✅ |
| Map | `(Map K V)` | ✅ |
| Any | `Any` | ✅ |

---

## Gap Analysis (Inspired by Zig)

### Priority 1: Control Flow (Critical)

Zig provides: `while`, `for`, `switch`, `break`, `continue`, `return`, `defer`, `errdefer`

**Missing in SQALE:**
```lisp
; NEEDED: while loop
[while [< i 10]
  [print i]
  [set! i [+ i 1]]]

; NEEDED: for/each iteration
[for [x items]
  [print x]]

; NEEDED: early return
[fn [[x : Int]] : Int
  [if [< x 0] [return 0]]
  [* x 2]]

; NEEDED: break/continue
[while true
  [if condition [break]]
  [if skip [continue]]
  [do-work]]

; NEEDED: defer for cleanup
[fn [] : Unit
  [let [[f [open-file "test.txt"]]]
    [defer [close f]]
    [read f]]]

; NEEDED: match/switch
[match value
  [0 "zero"]
  [1 "one"]
  [_ "other"]]
```

### Priority 2: Error Handling (Important)

Zig provides: `error`, `try`, `catch`, `orelse`, error unions

**Missing in SQALE:**
```lisp
; NEEDED: Result type
[def divide : [Int Int -> [Result Int Str]]
  [fn [[a : Int] [b : Int]] : [Result Int Str]
    [if [= b 0]
      [err "division by zero"]
      [ok [/ a b]]]]]

; NEEDED: try/catch or unwrap
[try [divide 10 0]
  [ok val [print val]]
  [err e [print e]]]

; NEEDED: Option type
[def find : [Vec Int -> [Option Int]]
  [fn [[xs : Vec]] : [Option Int]
    ...]]
```

### Priority 3: Extended Math/Bitwise Operations

Zig provides: `@mod`, `@rem`, `@clz`, `@ctz`, `@popCount`, bitwise ops

**Missing in SQALE:**
```lisp
; NEEDED: Bitwise operations
[bit-and 0xFF 0x0F]    ; bitwise AND
[bit-or 0x0F 0xF0]     ; bitwise OR
[bit-xor 0xFF 0x0F]    ; bitwise XOR
[bit-not 0xFF]         ; bitwise NOT
[shl 1 4]              ; shift left (= 16)
[shr 16 2]             ; shift right (= 4)

; NEEDED: Math functions
[abs -5]               ; absolute value
[min 3 7]              ; minimum
[max 3 7]              ; maximum
[pow 2 10]             ; power (= 1024)
[sqrt 16]              ; square root

; NEEDED: Float operations
[floor 3.7]            ; = 3.0
[ceil 3.2]             ; = 4.0
[round 3.5]            ; = 4.0
```

### Priority 4: Extended String Operations

**Missing in SQALE:**
```lisp
; NEEDED: String manipulation
[str-slice "hello" 1 3]      ; "el"
[str-index "hello" "l"]      ; 2
[str-find "hello" "ll"]      ; 2
[str-replace "hello" "l" "L"]; "heLLo"
[str-upper "hello"]          ; "HELLO"
[str-lower "HELLO"]          ; "hello"
[str-trim "  hello  "]       ; "hello"

; NEEDED: Conversions
[str-to-int "42"]            ; 42
[int-to-str 42]              ; "42"
[str-to-float "3.14"]        ; 3.14
[float-to-str 3.14]          ; "3.14"

; NEEDED: String predicates
[str-starts-with "hello" "he"]  ; true
[str-ends-with "hello" "lo"]    ; true
[str-contains "hello" "ell"]    ; true
```

### Priority 5: Data Structures

Zig provides: `struct`, `enum`, `union`, `packed`, `extern`

**Missing in SQALE:**
```lisp
; NEEDED: Struct/Record types
[defstruct Point
  [[x : Int]
   [y : Int]]]

[def p : Point [Point 10 20]]
[print [. p x]]  ; field access

; NEEDED: Enum/ADT
[defenum Color
  Red Green Blue
  [Rgb Int Int Int]]

; NEEDED: Tuple types
[def pair : [Tuple Int Str] [tuple 42 "hello"]]
```

### Priority 6: I/O Operations

**Missing in SQALE:**
```lisp
; NEEDED: File I/O
[read-file "path.txt"]         ; read entire file
[write-file "path.txt" content]; write to file
[file-exists? "path.txt"]      ; check existence
[delete-file "path.txt"]       ; delete file

; NEEDED: Console I/O
[read-line]                    ; read line from stdin
[print-err "error message"]    ; print to stderr

; NEEDED: Format strings
[format "Hello, {}!" name]     ; string interpolation
```

### Priority 7: System/Runtime

**Missing in SQALE:**
```lisp
; NEEDED: Type introspection
[type-of value]                ; get type at runtime
[is? value Int]                ; type check

; NEEDED: Assertions
[assert [> x 0] "x must be positive"]

; NEEDED: Environment
[env-get "HOME"]               ; get environment variable
[args]                         ; command line arguments

; NEEDED: Time
[time-now]                     ; current timestamp
[sleep 1000]                   ; sleep milliseconds
```

---

## Implementation Roadmap

### Phase 1: Core Control Flow (High Priority)
1. **`while` loop** - Essential for iteration
2. **`for` loop** - Iteration over collections
3. **`break`/`continue`** - Loop control
4. **`return`** - Early function return
5. **`set!`** - Variable mutation (with restrictions)

### Phase 2: Essential Built-ins
1. **Bitwise ops**: `bit-and`, `bit-or`, `bit-xor`, `bit-not`, `shl`, `shr`
2. **Math**: `abs`, `min`, `max`, `pow`, `sqrt`
3. **String**: `str-slice`, `str-index`, `str-to-int`, `int-to-str`
4. **I/O**: `read-line`, `read-file`, `write-file`

### Phase 3: Error Handling
1. **`Result` type** - `[Result T E]`
2. **`Option` type** - `[Option T]` or `?T`
3. **`try`/`catch`** - Error propagation
4. **`defer`** - Cleanup actions

### Phase 4: Data Structures
1. **Structs** - `defstruct`
2. **Enums/ADTs** - `defenum`
3. **Tuples** - `[Tuple T1 T2 ...]`
4. **Arrays** - Fixed-size vectors

### Phase 5: Advanced Features
1. **Pattern matching** - `match`/`switch`
2. **Generics** - Type parameters with constraints
3. **Type aliases** - `deftype`
4. **Compile-time evaluation** - Similar to Zig's `comptime`

---

## Comparison with Zig Keywords

| Zig Keyword | SQALE Equivalent | Status | Notes |
|-------------|------------------|--------|-------|
| `const` | `def` | ✅ | Immutable by default |
| `var` | `let` + `set!` | ⚠️ | Need mutation support |
| `fn` | `fn` | ✅ | |
| `if`/`else` | `if` | ✅ | |
| `while` | - | ❌ | **Needed** |
| `for` | - | ❌ | **Needed** |
| `switch` | - | ❌ | **Needed** (as `match`) |
| `break` | - | ❌ | **Needed** |
| `continue` | - | ❌ | **Needed** |
| `return` | - | ❌ | **Needed** |
| `defer` | - | ❌ | **Needed** |
| `try`/`catch` | - | ❌ | **Needed** |
| `struct` | - | ❌ | **Needed** |
| `enum` | - | ❌ | **Needed** |
| `union` | - | ❌ | Future |
| `comptime` | Macros | ✅ | Different approach |
| `pub`/`export` | - | ❌ | Module visibility |
| `inline` | - | ❌ | Optimization hint |
| `error` | - | ❌ | Error type |
| `true`/`false` | `true`/`false` | ✅ | |
| `null` | - | ❌ | Need Option type |
| `undefined` | - | ❌ | Not applicable |
| `and`/`or` | `and`/`or` | ✅ | |
| `async`/`await` | - | ❌ | Future (have channels) |

---

## Next Steps

1. Start with Phase 1 (Control Flow) - this unlocks many programming patterns
2. Add Phase 2 built-ins as they're needed
3. Phase 3 error handling enables robust programs
4. Phase 4+ are enhancements for larger programs

See `ROADMAP.md` for the overall project roadmap that incorporates these language features.
