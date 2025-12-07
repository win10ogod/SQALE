#ifndef CODEGEN_H
#define CODEGEN_H

#include <stddef.h>
#include <stdbool.h>
#include "ast.h"
#include "type.h"

// Codegen options
typedef struct CodegenOpts {
  const char *module_name;
  int use_llvm;       // 1 if compiled with LLVM C API
  int for_exe;        // emit main entrypoint
  int opt_level;      // 0-3, optimization level
  int emit_debug;     // emit debug info
} CodegenOpts;

// Emit textual LLVM IR for the program into an in-memory buffer.
// Returns a malloc'd buffer that the caller must free; size is stored in out_len.
char *codegen_emit_ir(Node *program, const CodegenOpts *opts, size_t *out_len);

// Compile to object file (requires USE_LLVM=1)
int codegen_emit_object(Node *program, const CodegenOpts *opts, const char *out_path);

// ============================================================================
// Internal codegen context (used by codegen_llvm.c)
// ============================================================================

// Symbol table entry for local variables
typedef struct CgSymbol {
  const char *name;
  size_t name_len;
  void *value;        // LLVMValueRef or text IR temp name
  Type *type;
  struct CgSymbol *next;
} CgSymbol;

// Symbol table (scope)
typedef struct CgScope {
  CgSymbol *symbols;
  struct CgScope *parent;
} CgScope;

// Codegen context
typedef struct CgContext {
  void *module;       // LLVMModuleRef or NULL
  void *builder;      // LLVMBuilderRef or NULL
  void *context;      // LLVMContextRef or NULL
  CgScope *scope;     // current scope
  Node *program;      // AST root
  CodegenOpts opts;

  // For text IR emission
  char *ir_buf;
  size_t ir_len;
  size_t ir_cap;
  char *globals_buf;
  size_t globals_len;
  size_t globals_cap;
  int tmp_id;
  int str_id;
  int label_id;

  // Function table for forward references
  struct {
    const char *name;
    size_t name_len;
    void *fn;         // LLVMValueRef
    Type *type;
  } *functions;
  int fn_count;
  int fn_cap;
} CgContext;

// Context management
CgContext *cg_context_new(const CodegenOpts *opts);
void cg_context_free(CgContext *ctx);

// Scope management
void cg_scope_push(CgContext *ctx);
void cg_scope_pop(CgContext *ctx);
void cg_scope_define(CgContext *ctx, const char *name, size_t len, void *val, Type *ty);
CgSymbol *cg_scope_lookup(CgContext *ctx, const char *name, size_t len);

// Code generation (returns LLVMValueRef or temp name index)
void *cg_expr(CgContext *ctx, Node *node);
void *cg_function(CgContext *ctx, Node *def);
void cg_program(CgContext *ctx, Node *program);

// ============================================================================
// Runtime function declarations for LLVM linking
// ============================================================================

// These are implemented in runtime.c and declared for LLVM linking
void sq_print_i64(long long v);
void sq_print_f64(double v);
void sq_print_bool(int v);
void sq_print_cstr(const char *s);
void sq_print_newline(void);

// Runtime allocation (for closures, etc.)
void *sq_alloc(size_t size);
void *sq_alloc_closure(void *fn, void *env, int arity);
void *sq_closure_get_fn(void *closure);
void *sq_closure_get_env(void *closure);

#endif // CODEGEN_H
