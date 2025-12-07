/*
 * SQALE LLVM Code Generator
 *
 * This file implements code generation from SQALE AST to LLVM IR.
 * It supports both:
 *   - USE_LLVM=1: Uses LLVM C API for IR generation
 *   - USE_LLVM=0: Emits textual LLVM IR
 *
 * Architecture:
 *   Source → Lexer → Parser → AST → Macro Expand → Type Check → [This] → LLVM IR
 *
 * References:
 *   - LLVM Kaleidoscope Tutorial: https://llvm.org/docs/tutorial/MyFirstLanguageFrontend/
 *   - LLVM C API: https://www.pauladamsmith.com/blog/2015/01/how-to-get-started-with-llvm-c-api.html
 */

#include "codegen.h"
#include "str.h"
#include "ast.h"
#include "type.h"
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <stdarg.h>

#if USE_LLVM
#include <llvm-c/Core.h>
#include <llvm-c/Analysis.h>
#include <llvm-c/Target.h>
#include <llvm-c/TargetMachine.h>
#include <llvm-c/Transforms/PassBuilder.h>
#endif

// ============================================================================
// Utility functions
// ============================================================================

static int is_sym(Node *n, const char *s) {
  if (!n || n->kind != N_SYMBOL) return 0;
  size_t len = strlen(s);
  return n->as.sym.len == len && strncmp(n->as.sym.ptr, s, len) == 0;
}

static int sym_eq(const char *a, size_t alen, const char *b, size_t blen) {
  return alen == blen && strncmp(a, b, alen) == 0;
}

// ============================================================================
// Text IR emission helpers (for USE_LLVM=0)
// ============================================================================

static void ir_ensure_cap(CgContext *ctx, size_t need) {
  if (ctx->ir_len + need >= ctx->ir_cap) {
    ctx->ir_cap = (ctx->ir_cap + need) * 2;
    ctx->ir_buf = (char*)realloc(ctx->ir_buf, ctx->ir_cap);
  }
}

static void ir_append(CgContext *ctx, const char *s) {
  size_t n = strlen(s);
  ir_ensure_cap(ctx, n + 1);
  memcpy(ctx->ir_buf + ctx->ir_len, s, n);
  ctx->ir_len += n;
  ctx->ir_buf[ctx->ir_len] = '\0';
}

static void ir_appendf(CgContext *ctx, const char *fmt, ...) {
  char buf[1024];
  va_list args;
  va_start(args, fmt);
  vsnprintf(buf, sizeof(buf), fmt, args);
  va_end(args);
  ir_append(ctx, buf);
}

static void glob_ensure_cap(CgContext *ctx, size_t need) {
  if (ctx->globals_len + need >= ctx->globals_cap) {
    ctx->globals_cap = (ctx->globals_cap + need) * 2;
    ctx->globals_buf = (char*)realloc(ctx->globals_buf, ctx->globals_cap);
  }
}

static void glob_append(CgContext *ctx, const char *s) {
  size_t n = strlen(s);
  glob_ensure_cap(ctx, n + 1);
  memcpy(ctx->globals_buf + ctx->globals_len, s, n);
  ctx->globals_len += n;
  ctx->globals_buf[ctx->globals_len] = '\0';
}

static void glob_appendf(CgContext *ctx, const char *fmt, ...) {
  char buf[1024];
  va_list args;
  va_start(args, fmt);
  vsnprintf(buf, sizeof(buf), fmt, args);
  va_end(args);
  glob_append(ctx, buf);
}

static int new_tmp(CgContext *ctx) {
  return ctx->tmp_id++;
}

static int new_label(CgContext *ctx) {
  return ctx->label_id++;
}

// ============================================================================
// Context and scope management
// ============================================================================

CgContext *cg_context_new(const CodegenOpts *opts) {
  CgContext *ctx = (CgContext*)calloc(1, sizeof(CgContext));
  if (opts) ctx->opts = *opts;

  ctx->ir_cap = 4096;
  ctx->ir_buf = (char*)malloc(ctx->ir_cap);
  ctx->ir_buf[0] = '\0';

  ctx->globals_cap = 1024;
  ctx->globals_buf = (char*)malloc(ctx->globals_cap);
  ctx->globals_buf[0] = '\0';

  ctx->fn_cap = 32;
  ctx->functions = calloc(ctx->fn_cap, sizeof(*ctx->functions));

  // Create initial global scope
  ctx->scope = (CgScope*)calloc(1, sizeof(CgScope));

  return ctx;
}

void cg_context_free(CgContext *ctx) {
  if (!ctx) return;

  // Free scopes
  while (ctx->scope) {
    CgScope *parent = ctx->scope->parent;
    CgSymbol *sym = ctx->scope->symbols;
    while (sym) {
      CgSymbol *next = sym->next;
      free(sym);
      sym = next;
    }
    free(ctx->scope);
    ctx->scope = parent;
  }

  free(ctx->ir_buf);
  free(ctx->globals_buf);
  free(ctx->functions);
  free(ctx);
}

void cg_scope_push(CgContext *ctx) {
  CgScope *scope = (CgScope*)calloc(1, sizeof(CgScope));
  scope->parent = ctx->scope;
  ctx->scope = scope;
}

void cg_scope_pop(CgContext *ctx) {
  if (!ctx->scope) return;
  CgScope *old = ctx->scope;
  ctx->scope = old->parent;

  CgSymbol *sym = old->symbols;
  while (sym) {
    CgSymbol *next = sym->next;
    free(sym);
    sym = next;
  }
  free(old);
}

void cg_scope_define(CgContext *ctx, const char *name, size_t len, void *val, Type *ty) {
  CgSymbol *sym = (CgSymbol*)calloc(1, sizeof(CgSymbol));
  sym->name = name;
  sym->name_len = len;
  sym->value = val;
  sym->type = ty;
  sym->next = ctx->scope->symbols;
  ctx->scope->symbols = sym;
}

CgSymbol *cg_scope_lookup(CgContext *ctx, const char *name, size_t len) {
  for (CgScope *s = ctx->scope; s; s = s->parent) {
    for (CgSymbol *sym = s->symbols; sym; sym = sym->next) {
      if (sym_eq(sym->name, sym->name_len, name, len)) {
        return sym;
      }
    }
  }
  return NULL;
}

// ============================================================================
// Type to LLVM type string conversion
// ============================================================================

static const char *type_to_llvm(Type *ty) {
  if (!ty) return "i64";
  switch (ty->kind) {
    case TY_INT: return "i64";
    case TY_FLOAT: return "double";
    case TY_BOOL: return "i1";
    case TY_STR: return "i8*";
    case TY_UNIT: return "void";
    case TY_ANY: return "i8*";
    case TY_FUNC: return "i8*";  // Function pointers as opaque
    case TY_CHAN: return "i8*";
    case TY_VEC: return "i8*";
    case TY_MAP: return "i8*";
    default: return "i64";
  }
}

static const char *type_to_llvm_ret(Type *ty) {
  if (!ty) return "i64";
  if (ty->kind == TY_UNIT) return "void";
  return type_to_llvm(ty);
}

// ============================================================================
// Text IR Code Generation
// ============================================================================

// Forward declarations
static int cg_expr_text(CgContext *ctx, Node *node);
static void cg_function_text(CgContext *ctx, Node *def);

// Emit runtime function declarations
static void emit_runtime_decls(CgContext *ctx) {
  ir_append(ctx, "; Runtime function declarations\n");
  ir_append(ctx, "declare void @sq_print_i64(i64)\n");
  ir_append(ctx, "declare void @sq_print_f64(double)\n");
  ir_append(ctx, "declare void @sq_print_bool(i1)\n");
  ir_append(ctx, "declare void @sq_print_cstr(i8*)\n");
  ir_append(ctx, "declare void @sq_print_newline()\n");
  ir_append(ctx, "declare i8* @sq_alloc(i64)\n");
  ir_append(ctx, "declare i8* @sq_alloc_closure(i8*, i8*, i32)\n");
  ir_append(ctx, "; String operations\n");
  ir_append(ctx, "declare i8* @sq_string_new(i8*, i64)\n");
  ir_append(ctx, "declare i8* @sq_string_concat(i8*, i8*)\n");
  ir_append(ctx, "declare i64 @sq_string_len(i8*)\n");
  ir_append(ctx, "\n");
}

// Generate code for integer literal
static int cg_int_text(CgContext *ctx, Node *node) {
  int t = new_tmp(ctx);
  ir_appendf(ctx, "  %%t%d = add i64 0, %lld\n", t, (long long)node->as.ival);
  return t;
}

// Generate code for float literal
static int cg_float_text(CgContext *ctx, Node *node) {
  int t = new_tmp(ctx);
  ir_appendf(ctx, "  %%t%d = fadd double 0.0, %g\n", t, node->as.fval);
  return t;
}

// Generate code for boolean literal
static int cg_bool_text(CgContext *ctx, Node *node) {
  int t = new_tmp(ctx);
  ir_appendf(ctx, "  %%t%d = add i1 0, %d\n", t, node->as.bval ? 1 : 0);
  return t;
}

// Generate code for string literal
static int cg_string_text(CgContext *ctx, Node *node) {
  int sid = ctx->str_id++;
  size_t len = node->as.str.len + 1;

  // Emit global string constant
  glob_appendf(ctx, "@.str%d = private unnamed_addr constant [%zu x i8] c\"", sid, len);
  for (size_t i = 0; i < node->as.str.len; i++) {
    char c = node->as.str.ptr[i];
    if (c == '\n') glob_append(ctx, "\\0A");
    else if (c == '\r') glob_append(ctx, "\\0D");
    else if (c == '\t') glob_append(ctx, "\\09");
    else if (c == '\\') glob_append(ctx, "\\5C");
    else if (c == '"') glob_append(ctx, "\\22");
    else if (c < 32 || c > 126) glob_appendf(ctx, "\\%02X", (unsigned char)c);
    else glob_appendf(ctx, "%c", c);
  }
  glob_append(ctx, "\\00\"\n");

  // Get pointer to string
  int t = new_tmp(ctx);
  ir_appendf(ctx, "  %%t%d = getelementptr inbounds [%zu x i8], [%zu x i8]* @.str%d, i64 0, i64 0\n",
             t, len, len, sid);
  return t;
}

// Generate code for symbol reference
static int cg_symbol_text(CgContext *ctx, Node *node) {
  CgSymbol *sym = cg_scope_lookup(ctx, node->as.sym.ptr, node->as.sym.len);
  if (!sym) {
    fprintf(stderr, "codegen: undefined symbol: %.*s\n",
            (int)node->as.sym.len, node->as.sym.ptr);
    return -1;
  }
  return (int)(intptr_t)sym->value;
}

// Generate code for binary operations
static int cg_binop_text(CgContext *ctx, const char *op, Node *left, Node *right, Type *ty) {
  int l = cg_expr_text(ctx, left);
  int r = cg_expr_text(ctx, right);
  if (l < 0 || r < 0) return -1;

  int t = new_tmp(ctx);
  const char *llvm_ty = type_to_llvm(ty);

  if (strcmp(op, "+") == 0) {
    if (ty && ty->kind == TY_FLOAT)
      ir_appendf(ctx, "  %%t%d = fadd %s %%t%d, %%t%d\n", t, llvm_ty, l, r);
    else
      ir_appendf(ctx, "  %%t%d = add %s %%t%d, %%t%d\n", t, llvm_ty, l, r);
  } else if (strcmp(op, "-") == 0) {
    if (ty && ty->kind == TY_FLOAT)
      ir_appendf(ctx, "  %%t%d = fsub %s %%t%d, %%t%d\n", t, llvm_ty, l, r);
    else
      ir_appendf(ctx, "  %%t%d = sub %s %%t%d, %%t%d\n", t, llvm_ty, l, r);
  } else if (strcmp(op, "*") == 0) {
    if (ty && ty->kind == TY_FLOAT)
      ir_appendf(ctx, "  %%t%d = fmul %s %%t%d, %%t%d\n", t, llvm_ty, l, r);
    else
      ir_appendf(ctx, "  %%t%d = mul %s %%t%d, %%t%d\n", t, llvm_ty, l, r);
  } else if (strcmp(op, "/") == 0) {
    if (ty && ty->kind == TY_FLOAT)
      ir_appendf(ctx, "  %%t%d = fdiv %s %%t%d, %%t%d\n", t, llvm_ty, l, r);
    else
      ir_appendf(ctx, "  %%t%d = sdiv %s %%t%d, %%t%d\n", t, llvm_ty, l, r);
  } else if (strcmp(op, "%") == 0) {
    ir_appendf(ctx, "  %%t%d = srem %s %%t%d, %%t%d\n", t, llvm_ty, l, r);
  } else if (strcmp(op, "=") == 0) {
    if (left->ty && left->ty->kind == TY_FLOAT)
      ir_appendf(ctx, "  %%t%d = fcmp oeq double %%t%d, %%t%d\n", t, l, r);
    else
      ir_appendf(ctx, "  %%t%d = icmp eq i64 %%t%d, %%t%d\n", t, l, r);
  } else if (strcmp(op, "<") == 0) {
    if (left->ty && left->ty->kind == TY_FLOAT)
      ir_appendf(ctx, "  %%t%d = fcmp olt double %%t%d, %%t%d\n", t, l, r);
    else
      ir_appendf(ctx, "  %%t%d = icmp slt i64 %%t%d, %%t%d\n", t, l, r);
  } else if (strcmp(op, ">") == 0) {
    if (left->ty && left->ty->kind == TY_FLOAT)
      ir_appendf(ctx, "  %%t%d = fcmp ogt double %%t%d, %%t%d\n", t, l, r);
    else
      ir_appendf(ctx, "  %%t%d = icmp sgt i64 %%t%d, %%t%d\n", t, l, r);
  } else if (strcmp(op, "<=") == 0) {
    if (left->ty && left->ty->kind == TY_FLOAT)
      ir_appendf(ctx, "  %%t%d = fcmp ole double %%t%d, %%t%d\n", t, l, r);
    else
      ir_appendf(ctx, "  %%t%d = icmp sle i64 %%t%d, %%t%d\n", t, l, r);
  } else if (strcmp(op, ">=") == 0) {
    if (left->ty && left->ty->kind == TY_FLOAT)
      ir_appendf(ctx, "  %%t%d = fcmp oge double %%t%d, %%t%d\n", t, l, r);
    else
      ir_appendf(ctx, "  %%t%d = icmp sge i64 %%t%d, %%t%d\n", t, l, r);
  } else if (strcmp(op, "!=") == 0) {
    if (left->ty && left->ty->kind == TY_FLOAT)
      ir_appendf(ctx, "  %%t%d = fcmp one double %%t%d, %%t%d\n", t, l, r);
    else
      ir_appendf(ctx, "  %%t%d = icmp ne i64 %%t%d, %%t%d\n", t, l, r);
  } else if (strcmp(op, "mod") == 0) {
    ir_appendf(ctx, "  %%t%d = srem i64 %%t%d, %%t%d\n", t, l, r);
  } else if (strcmp(op, "and") == 0) {
    ir_appendf(ctx, "  %%t%d = and i1 %%t%d, %%t%d\n", t, l, r);
  } else if (strcmp(op, "or") == 0) {
    ir_appendf(ctx, "  %%t%d = or i1 %%t%d, %%t%d\n", t, l, r);
  } else {
    fprintf(stderr, "codegen: unknown binary operator: %s\n", op);
    return -1;
  }

  return t;
}

// Generate code for unary operations
static int cg_unop_text(CgContext *ctx, const char *op, Node *arg) {
  int a = cg_expr_text(ctx, arg);
  if (a < 0) return -1;

  int t = new_tmp(ctx);

  if (strcmp(op, "not") == 0) {
    ir_appendf(ctx, "  %%t%d = xor i1 %%t%d, 1\n", t, a);
  } else if (strcmp(op, "neg") == 0) {
    if (arg->ty && arg->ty->kind == TY_FLOAT)
      ir_appendf(ctx, "  %%t%d = fneg double %%t%d\n", t, a);
    else
      ir_appendf(ctx, "  %%t%d = sub i64 0, %%t%d\n", t, a);
  } else {
    fprintf(stderr, "codegen: unknown unary operator: %s\n", op);
    return -1;
  }

  return t;
}

// Generate code for print call
static int cg_print_text(CgContext *ctx, Node *list) {
  for (size_t i = 1; i < list->as.list.count; i++) {
    Node *arg = list->as.list.items[i];
    int t = cg_expr_text(ctx, arg);
    if (t < 0) continue;

    Type *ty = arg->ty;
    if (!ty) ty = ty_int(NULL);

    switch (ty->kind) {
      case TY_INT:
        ir_appendf(ctx, "  call void @sq_print_i64(i64 %%t%d)\n", t);
        break;
      case TY_FLOAT:
        ir_appendf(ctx, "  call void @sq_print_f64(double %%t%d)\n", t);
        break;
      case TY_BOOL:
        ir_appendf(ctx, "  call void @sq_print_bool(i1 %%t%d)\n", t);
        break;
      case TY_STR:
        ir_appendf(ctx, "  call void @sq_print_cstr(i8* %%t%d)\n", t);
        break;
      default:
        ir_appendf(ctx, "  call void @sq_print_i64(i64 %%t%d)\n", t);
        break;
    }

    if (i + 1 < list->as.list.count) {
      // Print space between arguments
      // (simplified: just print each on same line)
    }
  }
  ir_append(ctx, "  call void @sq_print_newline()\n");
  return -1;  // print returns void
}

// Generate code for if expression
static int cg_if_text(CgContext *ctx, Node *list) {
  // [if cond then else]
  if (list->as.list.count != 4) {
    fprintf(stderr, "codegen: if requires 3 arguments\n");
    return -1;
  }

  Node *cond_node = list->as.list.items[1];
  Node *then_node = list->as.list.items[2];
  Node *else_node = list->as.list.items[3];

  int cond = cg_expr_text(ctx, cond_node);
  if (cond < 0) return -1;

  // Check if result type is void (Unit)
  Type *result_ty = list->ty;
  int is_void = result_ty && result_ty->kind == TY_UNIT;

  int then_label = new_label(ctx);
  int else_label = new_label(ctx);
  int merge_label = new_label(ctx);

  ir_appendf(ctx, "  br i1 %%t%d, label %%then%d, label %%else%d\n",
             cond, then_label, else_label);

  // Then block
  ir_appendf(ctx, "then%d:\n", then_label);
  int then_val = cg_expr_text(ctx, then_node);
  int then_end_tmp = -1;
  if (!is_void) {
    then_end_tmp = then_val >= 0 ? then_val : new_tmp(ctx);
    if (then_val < 0) {
      ir_appendf(ctx, "  %%t%d = add i64 0, 0\n", then_end_tmp);
    }
  }
  ir_appendf(ctx, "  br label %%merge%d\n", merge_label);

  // Else block
  ir_appendf(ctx, "else%d:\n", else_label);
  int else_val = cg_expr_text(ctx, else_node);
  int else_end_tmp = -1;
  if (!is_void) {
    else_end_tmp = else_val >= 0 ? else_val : new_tmp(ctx);
    if (else_val < 0) {
      ir_appendf(ctx, "  %%t%d = add i64 0, 0\n", else_end_tmp);
    }
  }
  ir_appendf(ctx, "  br label %%merge%d\n", merge_label);

  // Merge block
  ir_appendf(ctx, "merge%d:\n", merge_label);

  // Only emit phi if result is not void
  if (is_void) {
    return -1;  // void result
  }

  int result = new_tmp(ctx);
  const char *llvm_ty = type_to_llvm(result_ty);
  ir_appendf(ctx, "  %%t%d = phi %s [ %%t%d, %%then%d ], [ %%t%d, %%else%d ]\n",
             result, llvm_ty, then_end_tmp, then_label, else_end_tmp, else_label);

  return result;
}

// Generate code for let binding
static int cg_let_text(CgContext *ctx, Node *list) {
  // [let [[name : Type expr] ...] body...]
  if (list->as.list.count < 3) {
    fprintf(stderr, "codegen: let requires bindings and body\n");
    return -1;
  }

  Node *bindings = list->as.list.items[1];
  cg_scope_push(ctx);

  // Process bindings
  for (size_t i = 0; i < bindings->as.list.count; i++) {
    Node *binding = bindings->as.list.items[i];
    if (binding->kind != N_LIST || binding->as.list.count < 2) continue;

    Node *name_node = binding->as.list.items[0];
    Node *expr_node = NULL;
    Type *bind_ty = NULL;

    // Parse binding: [name : Type expr] or [name expr]
    if (binding->as.list.count >= 4 && is_sym(binding->as.list.items[1], ":")) {
      expr_node = binding->as.list.items[3];
      bind_ty = expr_node->ty;
    } else if (binding->as.list.count >= 2) {
      expr_node = binding->as.list.items[1];
      bind_ty = expr_node->ty;
    }

    if (expr_node) {
      int val = cg_expr_text(ctx, expr_node);
      cg_scope_define(ctx, name_node->as.sym.ptr, name_node->as.sym.len,
                      (void*)(intptr_t)val, bind_ty);
    }
  }

  // Evaluate body expressions
  int result = -1;
  for (size_t i = 2; i < list->as.list.count; i++) {
    result = cg_expr_text(ctx, list->as.list.items[i]);
  }

  cg_scope_pop(ctx);
  return result;
}

// Generate code for do block
static int cg_do_text(CgContext *ctx, Node *list) {
  int result = -1;
  for (size_t i = 1; i < list->as.list.count; i++) {
    result = cg_expr_text(ctx, list->as.list.items[i]);
  }
  return result;
}

// Generate code for function call
static int cg_call_text(CgContext *ctx, Node *list) {
  Node *head = list->as.list.items[0];

  // Check if it's a known function
  if (head->kind == N_SYMBOL) {
    char fname[256];
    snprintf(fname, sizeof(fname), "%.*s", (int)head->as.sym.len, head->as.sym.ptr);

    // Check for builtin binary operators
    if (strcmp(fname, "+") == 0 || strcmp(fname, "-") == 0 ||
        strcmp(fname, "*") == 0 || strcmp(fname, "/") == 0 ||
        strcmp(fname, "%") == 0 || strcmp(fname, "mod") == 0 ||
        strcmp(fname, "=") == 0 || strcmp(fname, "!=") == 0 ||
        strcmp(fname, "<") == 0 || strcmp(fname, ">") == 0 ||
        strcmp(fname, "<=") == 0 || strcmp(fname, ">=") == 0 ||
        strcmp(fname, "and") == 0 || strcmp(fname, "or") == 0) {
      if (list->as.list.count == 3) {
        Type *ty = list->ty ? list->ty : ty_int(NULL);
        return cg_binop_text(ctx, fname, list->as.list.items[1],
                             list->as.list.items[2], ty);
      }
    }

    // Check for unary operators
    if (strcmp(fname, "not") == 0 || strcmp(fname, "neg") == 0) {
      if (list->as.list.count == 2) {
        return cg_unop_text(ctx, fname, list->as.list.items[1]);
      }
    }

    // Check for print
    if (strcmp(fname, "print") == 0) {
      return cg_print_text(ctx, list);
    }

    // Check for string operations
    if (strcmp(fname, "str-concat") == 0 && list->as.list.count == 3) {
      int a = cg_expr_text(ctx, list->as.list.items[1]);
      int b = cg_expr_text(ctx, list->as.list.items[2]);
      if (a < 0 || b < 0) return -1;
      int t = new_tmp(ctx);
      ir_appendf(ctx, "  %%t%d = call i8* @sq_string_concat(i8* %%t%d, i8* %%t%d)\n", t, a, b);
      return t;
    }

    if (strcmp(fname, "str-len") == 0 && list->as.list.count == 2) {
      int a = cg_expr_text(ctx, list->as.list.items[1]);
      if (a < 0) return -1;
      int t = new_tmp(ctx);
      ir_appendf(ctx, "  %%t%d = call i64 @sq_string_len(i8* %%t%d)\n", t, a);
      return t;
    }

    // User-defined function call
    CgSymbol *sym = cg_scope_lookup(ctx, head->as.sym.ptr, head->as.sym.len);
    if (sym && sym->type && sym->type->kind == TY_FUNC) {
      // Generate call to user function
      int argc = (int)list->as.list.count - 1;
      int *args = (int*)malloc(sizeof(int) * argc);

      for (int i = 0; i < argc; i++) {
        args[i] = cg_expr_text(ctx, list->as.list.items[i + 1]);
      }

      // Build call instruction
      Type *ret_ty = sym->type->as.fn.ret;
      const char *ret_llvm = type_to_llvm_ret(ret_ty);
      int result = new_tmp(ctx);

      if (ret_ty && ret_ty->kind == TY_UNIT) {
        ir_appendf(ctx, "  call void @%s(", fname);
      } else {
        ir_appendf(ctx, "  %%t%d = call %s @%s(", result, ret_llvm, fname);
      }

      for (int i = 0; i < argc; i++) {
        Type *arg_ty = list->as.list.items[i + 1]->ty;
        ir_appendf(ctx, "%s %%t%d", type_to_llvm(arg_ty), args[i]);
        if (i + 1 < argc) ir_append(ctx, ", ");
      }
      ir_append(ctx, ")\n");

      free(args);
      return (ret_ty && ret_ty->kind == TY_UNIT) ? -1 : result;
    }
  }

  fprintf(stderr, "codegen: unknown function call\n");
  return -1;
}

// Generate code for list expression
static int cg_list_text(CgContext *ctx, Node *list) {
  if (list->as.list.count == 0) return -1;

  Node *head = list->as.list.items[0];

  // Special forms
  if (head->kind == N_SYMBOL) {
    if (is_sym(head, "if")) return cg_if_text(ctx, list);
    if (is_sym(head, "let")) return cg_let_text(ctx, list);
    if (is_sym(head, "do")) return cg_do_text(ctx, list);
    if (is_sym(head, "def")) return -1;  // Handled at top level
    if (is_sym(head, "fn")) return -1;   // Closure creation (TODO)
    if (is_sym(head, "quote")) return -1;
    if (is_sym(head, "quasiquote")) return -1;
    if (is_sym(head, "defmacro")) return -1;
    if (is_sym(head, "import")) return -1;
  }

  // Function call
  return cg_call_text(ctx, list);
}

// Main expression codegen dispatcher
static int cg_expr_text(CgContext *ctx, Node *node) {
  switch (node->kind) {
    case N_INT: return cg_int_text(ctx, node);
    case N_FLOAT: return cg_float_text(ctx, node);
    case N_BOOL: return cg_bool_text(ctx, node);
    case N_STRING: return cg_string_text(ctx, node);
    case N_SYMBOL: return cg_symbol_text(ctx, node);
    case N_LIST: return cg_list_text(ctx, node);
    default: return -1;
  }
}

// Generate code for function definition
static void cg_function_text(CgContext *ctx, Node *def) {
  // [def name : Type [fn [[param : T] ...] : RetT body...]]
  if (def->as.list.count < 5) return;

  Node *name_node = def->as.list.items[1];
  Node *fn_node = def->as.list.items[4];

  if (fn_node->kind != N_LIST || fn_node->as.list.count < 3) return;
  if (!is_sym(fn_node->as.list.items[0], "fn")) return;

  char fname[256];
  snprintf(fname, sizeof(fname), "%.*s", (int)name_node->as.sym.len, name_node->as.sym.ptr);

  Node *params = fn_node->as.list.items[1];
  Type *fn_type = fn_node->ty;
  Type *ret_type = fn_type && fn_type->kind == TY_FUNC ? fn_type->as.fn.ret : ty_int(NULL);

  // Emit function signature
  const char *ret_llvm = type_to_llvm_ret(ret_type);
  ir_appendf(ctx, "define %s @%s(", ret_llvm, fname);

  // Parameters
  cg_scope_push(ctx);
  for (size_t i = 0; i < params->as.list.count; i++) {
    Node *param = params->as.list.items[i];
    if (param->kind != N_LIST || param->as.list.count < 3) continue;

    Node *pname = param->as.list.items[0];
    Type *ptype = fn_type && i < fn_type->as.fn.arity ? fn_type->as.fn.params[i] : ty_int(NULL);

    int t = new_tmp(ctx);
    ir_appendf(ctx, "%s %%t%d", type_to_llvm(ptype), t);
    if (i + 1 < params->as.list.count) ir_append(ctx, ", ");

    cg_scope_define(ctx, pname->as.sym.ptr, pname->as.sym.len,
                    (void*)(intptr_t)t, ptype);
  }
  ir_append(ctx, ") {\n");
  ir_append(ctx, "entry:\n");

  // Register function in scope for recursion
  cg_scope_define(ctx, name_node->as.sym.ptr, name_node->as.sym.len, NULL, fn_type);

  // Function body
  size_t body_start = 2;
  if (fn_node->as.list.count > body_start && is_sym(fn_node->as.list.items[body_start], ":")) {
    body_start += 2;  // Skip : RetType
  }

  int last_result = -1;
  for (size_t i = body_start; i < fn_node->as.list.count; i++) {
    last_result = cg_expr_text(ctx, fn_node->as.list.items[i]);
  }

  // Return
  if (ret_type && ret_type->kind == TY_UNIT) {
    ir_append(ctx, "  ret void\n");
  } else if (last_result >= 0) {
    ir_appendf(ctx, "  ret %s %%t%d\n", ret_llvm, last_result);
  } else {
    ir_appendf(ctx, "  ret %s 0\n", ret_llvm);
  }

  ir_append(ctx, "}\n\n");
  cg_scope_pop(ctx);
}

// Find main function in program
static int find_main_fn(Node *program) {
  for (size_t i = 0; i < program->as.list.count; i++) {
    Node *f = program->as.list.items[i];
    if (f->kind != N_LIST || f->as.list.count < 5) continue;
    if (is_sym(f->as.list.items[0], "def") && is_sym(f->as.list.items[1], "main")) {
      return (int)i;
    }
  }
  return -1;
}

// First pass: register all function names
static void register_functions(CgContext *ctx, Node *program) {
  for (size_t i = 0; i < program->as.list.count; i++) {
    Node *form = program->as.list.items[i];
    if (form->kind != N_LIST || form->as.list.count < 5) continue;
    if (!is_sym(form->as.list.items[0], "def")) continue;

    Node *name = form->as.list.items[1];
    Node *fn_node = form->as.list.items[4];
    Type *fn_type = fn_node->ty;

    cg_scope_define(ctx, name->as.sym.ptr, name->as.sym.len, NULL, fn_type);
  }
}

// Generate IR for entire program
static void cg_program_text(CgContext *ctx, Node *program) {
  // Module header
  ir_append(ctx, "; ModuleID = 'sqale'\n");
  ir_appendf(ctx, "source_filename = \"%s\"\n",
             ctx->opts.module_name ? ctx->opts.module_name : "sqale");
  ir_append(ctx, "target triple = \"x86_64-unknown-linux-gnu\"\n\n");

  // Runtime declarations
  emit_runtime_decls(ctx);

  // Register all functions first (for forward references)
  register_functions(ctx, program);

  // Generate all functions
  for (size_t i = 0; i < program->as.list.count; i++) {
    Node *form = program->as.list.items[i];
    if (form->kind != N_LIST || form->as.list.count < 5) continue;
    if (!is_sym(form->as.list.items[0], "def")) continue;

    Node *fn_node = form->as.list.items[4];
    if (fn_node->kind == N_LIST && fn_node->as.list.count >= 3 &&
        is_sym(fn_node->as.list.items[0], "fn")) {
      cg_function_text(ctx, form);
    }
  }

  // If no main function exists, create a simple one
  if (find_main_fn(program) < 0 && ctx->opts.for_exe) {
    ir_append(ctx, "define i32 @main() {\n");
    ir_append(ctx, "entry:\n");
    ir_append(ctx, "  ret i32 0\n");
    ir_append(ctx, "}\n");
  }
}

// ============================================================================
// LLVM C API Code Generation (when USE_LLVM=1)
// ============================================================================

#if USE_LLVM

// LLVM codegen context extension
typedef struct {
  LLVMContextRef ctx;
  LLVMModuleRef module;
  LLVMBuilderRef builder;
  LLVMValueRef current_fn;
} LLVMCg;

static LLVMTypeRef type_to_llvm_type(LLVMContextRef ctx, Type *ty) {
  if (!ty) return LLVMInt64TypeInContext(ctx);
  switch (ty->kind) {
    case TY_INT: return LLVMInt64TypeInContext(ctx);
    case TY_FLOAT: return LLVMDoubleTypeInContext(ctx);
    case TY_BOOL: return LLVMInt1TypeInContext(ctx);
    case TY_STR: return LLVMPointerType(LLVMInt8TypeInContext(ctx), 0);
    case TY_UNIT: return LLVMVoidTypeInContext(ctx);
    default: return LLVMInt64TypeInContext(ctx);
  }
}

static LLVMValueRef cg_expr_llvm(CgContext *ctx, LLVMCg *llvm, Node *node);

static LLVMValueRef cg_int_llvm(LLVMCg *llvm, Node *node) {
  return LLVMConstInt(LLVMInt64TypeInContext(llvm->ctx), node->as.ival, 1);
}

static LLVMValueRef cg_float_llvm(LLVMCg *llvm, Node *node) {
  return LLVMConstReal(LLVMDoubleTypeInContext(llvm->ctx), node->as.fval);
}

static LLVMValueRef cg_bool_llvm(LLVMCg *llvm, Node *node) {
  return LLVMConstInt(LLVMInt1TypeInContext(llvm->ctx), node->as.bval ? 1 : 0, 0);
}

static LLVMValueRef cg_binop_llvm(CgContext *ctx, LLVMCg *llvm, const char *op,
                                   Node *left, Node *right, Type *ty) {
  LLVMValueRef l = cg_expr_llvm(ctx, llvm, left);
  LLVMValueRef r = cg_expr_llvm(ctx, llvm, right);
  if (!l || !r) return NULL;

  if (strcmp(op, "+") == 0) {
    if (ty && ty->kind == TY_FLOAT)
      return LLVMBuildFAdd(llvm->builder, l, r, "addtmp");
    return LLVMBuildAdd(llvm->builder, l, r, "addtmp");
  }
  if (strcmp(op, "-") == 0) {
    if (ty && ty->kind == TY_FLOAT)
      return LLVMBuildFSub(llvm->builder, l, r, "subtmp");
    return LLVMBuildSub(llvm->builder, l, r, "subtmp");
  }
  if (strcmp(op, "*") == 0) {
    if (ty && ty->kind == TY_FLOAT)
      return LLVMBuildFMul(llvm->builder, l, r, "multmp");
    return LLVMBuildMul(llvm->builder, l, r, "multmp");
  }
  if (strcmp(op, "/") == 0) {
    if (ty && ty->kind == TY_FLOAT)
      return LLVMBuildFDiv(llvm->builder, l, r, "divtmp");
    return LLVMBuildSDiv(llvm->builder, l, r, "divtmp");
  }
  if (strcmp(op, "<") == 0) {
    if (ty && ty->kind == TY_FLOAT)
      return LLVMBuildFCmp(llvm->builder, LLVMRealOLT, l, r, "cmptmp");
    return LLVMBuildICmp(llvm->builder, LLVMIntSLT, l, r, "cmptmp");
  }
  if (strcmp(op, ">") == 0) {
    if (ty && ty->kind == TY_FLOAT)
      return LLVMBuildFCmp(llvm->builder, LLVMRealOGT, l, r, "cmptmp");
    return LLVMBuildICmp(llvm->builder, LLVMIntSGT, l, r, "cmptmp");
  }
  if (strcmp(op, "=") == 0) {
    if (left->ty && left->ty->kind == TY_FLOAT)
      return LLVMBuildFCmp(llvm->builder, LLVMRealOEQ, l, r, "cmptmp");
    return LLVMBuildICmp(llvm->builder, LLVMIntEQ, l, r, "cmptmp");
  }

  return NULL;
}

static LLVMValueRef cg_if_llvm(CgContext *ctx, LLVMCg *llvm, Node *list) {
  if (list->as.list.count != 4) return NULL;

  LLVMValueRef cond = cg_expr_llvm(ctx, llvm, list->as.list.items[1]);
  if (!cond) return NULL;

  LLVMBasicBlockRef then_bb = LLVMAppendBasicBlockInContext(llvm->ctx, llvm->current_fn, "then");
  LLVMBasicBlockRef else_bb = LLVMAppendBasicBlockInContext(llvm->ctx, llvm->current_fn, "else");
  LLVMBasicBlockRef merge_bb = LLVMAppendBasicBlockInContext(llvm->ctx, llvm->current_fn, "merge");

  LLVMBuildCondBr(llvm->builder, cond, then_bb, else_bb);

  // Then block
  LLVMPositionBuilderAtEnd(llvm->builder, then_bb);
  LLVMValueRef then_val = cg_expr_llvm(ctx, llvm, list->as.list.items[2]);
  LLVMBuildBr(llvm->builder, merge_bb);
  then_bb = LLVMGetInsertBlock(llvm->builder);

  // Else block
  LLVMPositionBuilderAtEnd(llvm->builder, else_bb);
  LLVMValueRef else_val = cg_expr_llvm(ctx, llvm, list->as.list.items[3]);
  LLVMBuildBr(llvm->builder, merge_bb);
  else_bb = LLVMGetInsertBlock(llvm->builder);

  // Merge with PHI
  LLVMPositionBuilderAtEnd(llvm->builder, merge_bb);
  LLVMTypeRef phi_ty = type_to_llvm_type(llvm->ctx, list->ty);
  LLVMValueRef phi = LLVMBuildPhi(llvm->builder, phi_ty, "iftmp");

  LLVMValueRef incoming_vals[] = { then_val, else_val };
  LLVMBasicBlockRef incoming_bbs[] = { then_bb, else_bb };
  LLVMAddIncoming(phi, incoming_vals, incoming_bbs, 2);

  return phi;
}

static LLVMValueRef cg_list_llvm(CgContext *ctx, LLVMCg *llvm, Node *list) {
  if (list->as.list.count == 0) return NULL;

  Node *head = list->as.list.items[0];
  if (head->kind == N_SYMBOL) {
    // Check for special forms
    if (is_sym(head, "if")) return cg_if_llvm(ctx, llvm, list);

    // Check for binary operators
    char op[32];
    snprintf(op, sizeof(op), "%.*s", (int)head->as.sym.len, head->as.sym.ptr);

    if ((strcmp(op, "+") == 0 || strcmp(op, "-") == 0 ||
         strcmp(op, "*") == 0 || strcmp(op, "/") == 0 ||
         strcmp(op, "<") == 0 || strcmp(op, ">") == 0 ||
         strcmp(op, "=") == 0) && list->as.list.count == 3) {
      return cg_binop_llvm(ctx, llvm, op, list->as.list.items[1],
                           list->as.list.items[2], list->ty);
    }
  }

  return NULL;
}

static LLVMValueRef cg_expr_llvm(CgContext *ctx, LLVMCg *llvm, Node *node) {
  switch (node->kind) {
    case N_INT: return cg_int_llvm(llvm, node);
    case N_FLOAT: return cg_float_llvm(llvm, node);
    case N_BOOL: return cg_bool_llvm(llvm, node);
    case N_LIST: return cg_list_llvm(ctx, llvm, node);
    default: return NULL;
  }
}

static char *codegen_emit_ir_llvm(Node *program, const CodegenOpts *opts, size_t *out_len) {
  LLVMContextRef ctx = LLVMContextCreate();
  LLVMModuleRef module = LLVMModuleCreateWithNameInContext(
    opts && opts->module_name ? opts->module_name : "sqale", ctx);
  LLVMBuilderRef builder = LLVMCreateBuilderInContext(ctx);

  LLVMCg llvm = { .ctx = ctx, .module = module, .builder = builder };

  // Create main function
  LLVMTypeRef main_ty = LLVMFunctionType(LLVMInt32TypeInContext(ctx), NULL, 0, 0);
  LLVMValueRef main_fn = LLVMAddFunction(module, "main", main_ty);
  llvm.current_fn = main_fn;

  LLVMBasicBlockRef entry = LLVMAppendBasicBlockInContext(ctx, main_fn, "entry");
  LLVMPositionBuilderAtEnd(builder, entry);

  // Generate code for program
  CgContext *cgctx = cg_context_new(opts);

  int mi = find_main_fn(program);
  LLVMValueRef ret_val = NULL;

  if (mi >= 0) {
    Node *main_def = program->as.list.items[mi];
    Node *fn_node = main_def->as.list.items[4];

    // Generate body
    size_t body_start = 2;
    if (fn_node->as.list.count > body_start && is_sym(fn_node->as.list.items[body_start], ":")) {
      body_start += 2;
    }

    for (size_t i = body_start; i < fn_node->as.list.count; i++) {
      ret_val = cg_expr_llvm(cgctx, &llvm, fn_node->as.list.items[i]);
    }
  }

  // Return
  if (ret_val && LLVMTypeOf(ret_val) == LLVMInt64TypeInContext(ctx)) {
    LLVMValueRef ret32 = LLVMBuildTrunc(builder, ret_val, LLVMInt32TypeInContext(ctx), "ret32");
    LLVMBuildRet(builder, ret32);
  } else {
    LLVMBuildRet(builder, LLVMConstInt(LLVMInt32TypeInContext(ctx), 0, 0));
  }

  cg_context_free(cgctx);

  // Get IR string
  char *ir = LLVMPrintModuleToString(module);
  if (out_len) *out_len = strlen(ir);

  LLVMDisposeBuilder(builder);
  LLVMDisposeModule(module);
  LLVMContextDispose(ctx);

  return ir;
}

#endif // USE_LLVM

// ============================================================================
// Public API
// ============================================================================

char *codegen_emit_ir(Node *program, const CodegenOpts *opts, size_t *out_len) {
#if USE_LLVM
  if (opts && opts->use_llvm) {
    return codegen_emit_ir_llvm(program, opts, out_len);
  }
#endif

  // Text IR emission
  CgContext *ctx = cg_context_new(opts);
  ctx->program = program;

  cg_program_text(ctx, program);

  // Combine globals and main IR
  size_t total_len = ctx->globals_len + ctx->ir_len;
  char *result = (char*)malloc(total_len + 1);

  // Insert globals after header
  char *header_end = strstr(ctx->ir_buf, "; Runtime");
  if (header_end && ctx->globals_len > 0) {
    size_t header_len = header_end - ctx->ir_buf;
    memcpy(result, ctx->ir_buf, header_len);
    memcpy(result + header_len, ctx->globals_buf, ctx->globals_len);
    memcpy(result + header_len + ctx->globals_len, header_end, ctx->ir_len - header_len);
    result[total_len] = '\0';
  } else {
    memcpy(result, ctx->ir_buf, ctx->ir_len);
    result[ctx->ir_len] = '\0';
    total_len = ctx->ir_len;
  }

  if (out_len) *out_len = total_len;

  cg_context_free(ctx);
  return result;
}

int codegen_emit_object(Node *program, const CodegenOpts *opts, const char *out_path) {
#if USE_LLVM
  (void)program;
  (void)opts;
  (void)out_path;
  // TODO: Implement object file emission using LLVM target machine
  return 1;
#else
  (void)program;
  (void)opts;
  (void)out_path;
  fprintf(stderr, "Object file emission requires LLVM (compile with USE_LLVM=1)\n");
  return 1;
#endif
}
