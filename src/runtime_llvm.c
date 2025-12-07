/*
 * SQALE LLVM Runtime Library
 *
 * This file provides runtime functions that are linked with LLVM-compiled
 * SQALE programs. It's a standalone file that can be compiled separately:
 *
 *   clang -c runtime_llvm.c -o runtime_llvm.o
 *   clang program.ll runtime_llvm.o -o program
 *
 * Or compiled to a static library:
 *
 *   ar rcs libsqale_rt.a runtime_llvm.o
 *   clang program.ll -L. -lsqale_rt -o program
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

// ============================================================================
// Print Functions
// ============================================================================

void sq_print_i64(long long v) {
  printf("%lld", v);
}

void sq_print_f64(double v) {
  printf("%g", v);
}

void sq_print_bool(int v) {
  printf(v ? "true" : "false");
}

void sq_print_cstr(const char *s) {
  if (s) printf("%s", s);
}

void sq_print_newline(void) {
  printf("\n");
}

// ============================================================================
// Memory Allocation
// ============================================================================

void *sq_alloc(size_t size) {
  return calloc(1, size);
}

void sq_free(void *ptr) {
  free(ptr);
}

// ============================================================================
// Closure Support
//
// Closures are represented as:
//   struct { void *fn; void *env; int32_t arity; }
//
// The 'fn' pointer points to the actual function.
// The 'env' pointer points to the captured environment (or NULL).
// The 'arity' field stores the number of parameters.
// ============================================================================

typedef struct {
  void *fn;
  void *env;
  int32_t arity;
} SqClosure;

void *sq_alloc_closure(void *fn, void *env, int arity) {
  SqClosure *c = (SqClosure*)calloc(1, sizeof(SqClosure));
  c->fn = fn;
  c->env = env;
  c->arity = arity;
  return c;
}

void *sq_closure_get_fn(void *closure) {
  if (!closure) return NULL;
  return ((SqClosure*)closure)->fn;
}

void *sq_closure_get_env(void *closure) {
  if (!closure) return NULL;
  return ((SqClosure*)closure)->env;
}

int sq_closure_get_arity(void *closure) {
  if (!closure) return 0;
  return ((SqClosure*)closure)->arity;
}

// ============================================================================
// String Operations
// ============================================================================

// Allocate a new string (copies data)
char *sq_string_new(const char *data, size_t len) {
  char *s = (char*)malloc(len + 1);
  if (data) memcpy(s, data, len);
  s[len] = '\0';
  return s;
}

// Concatenate two strings
char *sq_string_concat(const char *a, const char *b) {
  size_t la = a ? strlen(a) : 0;
  size_t lb = b ? strlen(b) : 0;
  char *s = (char*)malloc(la + lb + 1);
  if (a) memcpy(s, a, la);
  if (b) memcpy(s + la, b, lb);
  s[la + lb] = '\0';
  return s;
}

// String length
size_t sq_string_len(const char *s) {
  return s ? strlen(s) : 0;
}

// ============================================================================
// Vector Operations (dynamic arrays)
// ============================================================================

typedef struct {
  int64_t *items;
  int32_t len;
  int32_t cap;
} SqVec;

void *sq_vec_new(void) {
  SqVec *v = (SqVec*)calloc(1, sizeof(SqVec));
  v->cap = 8;
  v->items = (int64_t*)calloc(v->cap, sizeof(int64_t));
  return v;
}

void sq_vec_push(void *vec, int64_t val) {
  SqVec *v = (SqVec*)vec;
  if (!v) return;
  if (v->len >= v->cap) {
    v->cap *= 2;
    v->items = (int64_t*)realloc(v->items, v->cap * sizeof(int64_t));
  }
  v->items[v->len++] = val;
}

int64_t sq_vec_get(void *vec, int64_t idx) {
  SqVec *v = (SqVec*)vec;
  if (!v || idx < 0 || idx >= v->len) return 0;
  return v->items[idx];
}

int64_t sq_vec_len(void *vec) {
  SqVec *v = (SqVec*)vec;
  return v ? v->len : 0;
}

// ============================================================================
// Comparison Operations (for polymorphic equality)
// ============================================================================

int sq_eq_i64(int64_t a, int64_t b) {
  return a == b;
}

int sq_lt_i64(int64_t a, int64_t b) {
  return a < b;
}

int sq_gt_i64(int64_t a, int64_t b) {
  return a > b;
}

// ============================================================================
// Error Handling
// ============================================================================

void sq_panic(const char *msg) {
  fprintf(stderr, "SQALE panic: %s\n", msg);
  exit(1);
}

// ============================================================================
// Main Entry (if needed)
// ============================================================================

// If the compiled program defines 'sqale_main', call it
// extern int64_t sqale_main(void);
// int main(void) {
//   return (int)sqale_main();
// }
