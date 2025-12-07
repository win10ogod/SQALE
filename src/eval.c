#include "eval.h"
#include "parser.h"
#include "str.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <alloca.h>
#include <strings.h>

// Forward decls
static Value eval_node(VM *vm, Env *env, Node *n);
static int typecheck_node(Env *tenv, Node *n);
static Type *parse_type_node(Node *n);
// Macro expansion (applied in main.c before typecheck), but we also support quote at runtime
static int vm_import_file_impl(VM *vm, const char *path);
static int vm_import_resolve_and_load(VM *vm, const char *name);

VM *vm_new(void) {
  VM *vm = (VM*)calloc(1, sizeof(VM));
  gc_init(&vm->gc);
  vm->global_env = env_new(NULL);
  vm->global_env->aux = vm;

  // Register builtins (boxed values stored in env)
  Type *t_i = ty_int(NULL), *t_u = ty_unit(NULL), *t_any = ty_any(NULL);
  Value *vb;
  vb = (Value*)malloc(sizeof(Value)); *vb = v_native(rt_print, ty_func(NULL, (Type*[]){ t_any }, 1, t_u));
  env_set(vm->global_env, "print", vb->as.native.type, vb);
  vb = (Value*)malloc(sizeof(Value)); *vb = v_native(rt_add, ty_func(NULL, (Type*[]){t_i,t_i},2,t_i));
  env_set(vm->global_env, "+", vb->as.native.type, vb);
  vb = (Value*)malloc(sizeof(Value)); *vb = v_native(rt_sub, ty_func(NULL, (Type*[]){t_i,t_i},2,t_i));
  env_set(vm->global_env, "-", vb->as.native.type, vb);
  vb = (Value*)malloc(sizeof(Value)); *vb = v_native(rt_mul, ty_func(NULL, (Type*[]){t_i,t_i},2,t_i));
  env_set(vm->global_env, "*", vb->as.native.type, vb);
  vb = (Value*)malloc(sizeof(Value)); *vb = v_native(rt_div, ty_func(NULL, (Type*[]){t_i,t_i},2,t_i));
  env_set(vm->global_env, "/", vb->as.native.type, vb);
  vb = (Value*)malloc(sizeof(Value)); *vb = v_native(rt_eq, ty_func(NULL, (Type*[]){ty_any(NULL),ty_any(NULL)},2,ty_bool(NULL))); env_set(vm->global_env, "=", vb->as.native.type, vb);
  vb = (Value*)malloc(sizeof(Value)); *vb = v_native(rt_lt, ty_func(NULL, (Type*[]){t_i,t_i},2,ty_bool(NULL))); env_set(vm->global_env, "<", vb->as.native.type, vb);
  vb = (Value*)malloc(sizeof(Value)); *vb = v_native(rt_gt, ty_func(NULL, (Type*[]){t_i,t_i},2,ty_bool(NULL))); env_set(vm->global_env, ">", vb->as.native.type, vb);
  vb = (Value*)malloc(sizeof(Value)); *vb = v_native(rt_le, ty_func(NULL, (Type*[]){t_i,t_i},2,ty_bool(NULL))); env_set(vm->global_env, "<=", vb->as.native.type, vb);
  vb = (Value*)malloc(sizeof(Value)); *vb = v_native(rt_ge, ty_func(NULL, (Type*[]){t_i,t_i},2,ty_bool(NULL))); env_set(vm->global_env, ">=", vb->as.native.type, vb);
  vb = (Value*)malloc(sizeof(Value)); *vb = v_native(rt_ne, ty_func(NULL, (Type*[]){ty_any(NULL),ty_any(NULL)},2,ty_bool(NULL))); env_set(vm->global_env, "!=", vb->as.native.type, vb);
  // Logical operators
  vb = (Value*)malloc(sizeof(Value)); *vb = v_native(rt_not, ty_func(NULL, (Type*[]){ty_bool(NULL)},1,ty_bool(NULL))); env_set(vm->global_env, "not", vb->as.native.type, vb);
  vb = (Value*)malloc(sizeof(Value)); *vb = v_native(rt_and, ty_func(NULL, (Type*[]){ty_bool(NULL),ty_bool(NULL)},2,ty_bool(NULL))); env_set(vm->global_env, "and", vb->as.native.type, vb);
  vb = (Value*)malloc(sizeof(Value)); *vb = v_native(rt_or, ty_func(NULL, (Type*[]){ty_bool(NULL),ty_bool(NULL)},2,ty_bool(NULL))); env_set(vm->global_env, "or", vb->as.native.type, vb);
  // Modulo and negation
  vb = (Value*)malloc(sizeof(Value)); *vb = v_native(rt_mod, ty_func(NULL, (Type*[]){t_i,t_i},2,t_i)); env_set(vm->global_env, "mod", vb->as.native.type, vb);
  vb = (Value*)malloc(sizeof(Value)); *vb = v_native(rt_mod, ty_func(NULL, (Type*[]){t_i,t_i},2,t_i)); env_set(vm->global_env, "%", vb->as.native.type, vb);
  vb = (Value*)malloc(sizeof(Value)); *vb = v_native(rt_neg, ty_func(NULL, (Type*[]){t_i},1,t_i)); env_set(vm->global_env, "neg", vb->as.native.type, vb);
  // String operations
  Type *t_s = ty_str(NULL);
  vb = (Value*)malloc(sizeof(Value)); *vb = v_native(rt_str_concat, ty_func(NULL, (Type*[]){t_s,t_s},2,t_s)); env_set(vm->global_env, "str-concat", vb->as.native.type, vb);
  vb = (Value*)malloc(sizeof(Value)); *vb = v_native(rt_str_len, ty_func(NULL, (Type*[]){t_s},1,t_i)); env_set(vm->global_env, "str-len", vb->as.native.type, vb);
  vb = (Value*)malloc(sizeof(Value)); *vb = v_native(rt_str_slice, ty_func(NULL, (Type*[]){t_s,t_i,t_i},3,t_s)); env_set(vm->global_env, "str-slice", vb->as.native.type, vb);
  vb = (Value*)malloc(sizeof(Value)); *vb = v_native(rt_str_index, ty_func(NULL, (Type*[]){t_s,t_s},2,t_i)); env_set(vm->global_env, "str-index", vb->as.native.type, vb);
  // Bitwise operations
  vb = (Value*)malloc(sizeof(Value)); *vb = v_native(rt_bit_and, ty_func(NULL, (Type*[]){t_i,t_i},2,t_i)); env_set(vm->global_env, "bit-and", vb->as.native.type, vb);
  vb = (Value*)malloc(sizeof(Value)); *vb = v_native(rt_bit_or, ty_func(NULL, (Type*[]){t_i,t_i},2,t_i)); env_set(vm->global_env, "bit-or", vb->as.native.type, vb);
  vb = (Value*)malloc(sizeof(Value)); *vb = v_native(rt_bit_xor, ty_func(NULL, (Type*[]){t_i,t_i},2,t_i)); env_set(vm->global_env, "bit-xor", vb->as.native.type, vb);
  vb = (Value*)malloc(sizeof(Value)); *vb = v_native(rt_bit_not, ty_func(NULL, (Type*[]){t_i},1,t_i)); env_set(vm->global_env, "bit-not", vb->as.native.type, vb);
  vb = (Value*)malloc(sizeof(Value)); *vb = v_native(rt_shl, ty_func(NULL, (Type*[]){t_i,t_i},2,t_i)); env_set(vm->global_env, "shl", vb->as.native.type, vb);
  vb = (Value*)malloc(sizeof(Value)); *vb = v_native(rt_shr, ty_func(NULL, (Type*[]){t_i,t_i},2,t_i)); env_set(vm->global_env, "shr", vb->as.native.type, vb);
  // Extended math
  Type *t_f = ty_float(NULL);
  vb = (Value*)malloc(sizeof(Value)); *vb = v_native(rt_abs, ty_func(NULL, (Type*[]){t_i},1,t_i)); env_set(vm->global_env, "abs", vb->as.native.type, vb);
  vb = (Value*)malloc(sizeof(Value)); *vb = v_native(rt_min, ty_func(NULL, (Type*[]){t_i,t_i},2,t_i)); env_set(vm->global_env, "min", vb->as.native.type, vb);
  vb = (Value*)malloc(sizeof(Value)); *vb = v_native(rt_max, ty_func(NULL, (Type*[]){t_i,t_i},2,t_i)); env_set(vm->global_env, "max", vb->as.native.type, vb);
  vb = (Value*)malloc(sizeof(Value)); *vb = v_native(rt_pow, ty_func(NULL, (Type*[]){t_f,t_f},2,t_f)); env_set(vm->global_env, "pow", vb->as.native.type, vb);
  vb = (Value*)malloc(sizeof(Value)); *vb = v_native(rt_sqrt, ty_func(NULL, (Type*[]){t_f},1,t_f)); env_set(vm->global_env, "sqrt", vb->as.native.type, vb);
  vb = (Value*)malloc(sizeof(Value)); *vb = v_native(rt_floor, ty_func(NULL, (Type*[]){t_f},1,t_f)); env_set(vm->global_env, "floor", vb->as.native.type, vb);
  vb = (Value*)malloc(sizeof(Value)); *vb = v_native(rt_ceil, ty_func(NULL, (Type*[]){t_f},1,t_f)); env_set(vm->global_env, "ceil", vb->as.native.type, vb);
  vb = (Value*)malloc(sizeof(Value)); *vb = v_native(rt_round, ty_func(NULL, (Type*[]){t_f},1,t_f)); env_set(vm->global_env, "round", vb->as.native.type, vb);
  // String conversions
  vb = (Value*)malloc(sizeof(Value)); *vb = v_native(rt_str_to_int, ty_func(NULL, (Type*[]){t_s},1,t_i)); env_set(vm->global_env, "str-to-int", vb->as.native.type, vb);
  vb = (Value*)malloc(sizeof(Value)); *vb = v_native(rt_int_to_str, ty_func(NULL, (Type*[]){t_i},1,t_s)); env_set(vm->global_env, "int-to-str", vb->as.native.type, vb);
  vb = (Value*)malloc(sizeof(Value)); *vb = v_native(rt_str_to_float, ty_func(NULL, (Type*[]){t_s},1,t_f)); env_set(vm->global_env, "str-to-float", vb->as.native.type, vb);
  vb = (Value*)malloc(sizeof(Value)); *vb = v_native(rt_float_to_str, ty_func(NULL, (Type*[]){t_f},1,t_s)); env_set(vm->global_env, "float-to-str", vb->as.native.type, vb);
  vb = (Value*)malloc(sizeof(Value)); *vb = v_native(rt_chan, ty_func(NULL, (Type*[]){},0, ty_chan(NULL, t_i)));
  env_set(vm->global_env, "chan", vb->as.native.type, vb);
  vb = (Value*)malloc(sizeof(Value)); *vb = v_native(rt_send, ty_func(NULL, (Type*[]){ ty_chan(NULL, t_i), t_i }, 2, ty_bool(NULL)));
  env_set(vm->global_env, "send", vb->as.native.type, vb);
  vb = (Value*)malloc(sizeof(Value)); *vb = v_native(rt_recv, ty_func(NULL, (Type*[]){ ty_chan(NULL, t_i) }, 1, t_i));
  env_set(vm->global_env, "recv", vb->as.native.type, vb);
  Type *fn_u_u = ty_func(NULL, (Type*[]){}, 0, t_u); // Unit->Unit
  vb = (Value*)malloc(sizeof(Value)); *vb = v_native(rt_spawn, ty_func(NULL, (Type*[]){ fn_u_u }, 1, t_u));
  env_set(vm->global_env, "spawn", vb->as.native.type, vb);

  // Collections builtins (untyped/Any for simplicity)
  vb = (Value*)malloc(sizeof(Value)); *vb = v_native(rt_vec_new, ty_func(NULL, (Type*[]){}, 0, ty_vec(NULL, ty_any(NULL)))); env_set(vm->global_env, "vec", vb->as.native.type, vb);
  vb = (Value*)malloc(sizeof(Value)); *vb = v_native(rt_vec_push, ty_func(NULL, (Type*[]){ty_vec(NULL,ty_any(NULL)),ty_any(NULL)},2, t_u)); env_set(vm->global_env, "vec-push", vb->as.native.type, vb);
  vb = (Value*)malloc(sizeof(Value)); *vb = v_native(rt_vec_get, ty_func(NULL, (Type*[]){ty_vec(NULL,ty_any(NULL)), t_i},2, ty_any(NULL))); env_set(vm->global_env, "vec-get", vb->as.native.type, vb);
  vb = (Value*)malloc(sizeof(Value)); *vb = v_native(rt_vec_len, ty_func(NULL, (Type*[]){ty_vec(NULL,ty_any(NULL))},1, t_i)); env_set(vm->global_env, "vec-len", vb->as.native.type, vb);
  // Macro list/symbol helpers
  vb = (Value*)malloc(sizeof(Value)); *vb = v_native(rt_is_list, ty_func(NULL, (Type*[]){t_any},1, ty_bool(NULL))); env_set(vm->global_env, "list?", vb->as.native.type, vb);
  vb = (Value*)malloc(sizeof(Value)); *vb = v_native(rt_is_symbol, ty_func(NULL, (Type*[]){t_any},1, ty_bool(NULL))); env_set(vm->global_env, "symbol?", vb->as.native.type, vb);
  vb = (Value*)malloc(sizeof(Value)); *vb = v_native(rt_symbol_eq, ty_func(NULL, (Type*[]){t_any,t_any},2, ty_bool(NULL))); env_set(vm->global_env, "symbol=", vb->as.native.type, vb);
  vb = (Value*)malloc(sizeof(Value)); *vb = v_native(rt_list_len, ty_func(NULL, (Type*[]){t_any},1, t_i)); env_set(vm->global_env, "list-len", vb->as.native.type, vb);
  vb = (Value*)malloc(sizeof(Value)); *vb = v_native(rt_list_head, ty_func(NULL, (Type*[]){t_any},1, t_any)); env_set(vm->global_env, "list-head", vb->as.native.type, vb);
  vb = (Value*)malloc(sizeof(Value)); *vb = v_native(rt_list_tail, ty_func(NULL, (Type*[]){t_any},1, t_any)); env_set(vm->global_env, "list-tail", vb->as.native.type, vb);
  vb = (Value*)malloc(sizeof(Value)); *vb = v_native(rt_list_cons, ty_func(NULL, (Type*[]){t_any,t_any},2, t_any)); env_set(vm->global_env, "list-cons", vb->as.native.type, vb);
  vb = (Value*)malloc(sizeof(Value)); *vb = v_native(rt_list_append, ty_func(NULL, (Type*[]){t_any,t_any},2, t_any)); env_set(vm->global_env, "list-append", vb->as.native.type, vb);
  // Strings
  vb = (Value*)malloc(sizeof(Value)); *vb = v_native(rt_str_split_ws, ty_func(NULL, (Type*[]){t_any},1, ty_vec(NULL, ty_str(NULL)))); env_set(vm->global_env, "str-split-ws", vb->as.native.type, vb);
  // Maps
  vb = (Value*)malloc(sizeof(Value)); *vb = v_native(rt_map_new, ty_func(NULL, (Type*[]){},0, ty_map(NULL, ty_str(NULL), t_i))); env_set(vm->global_env, "map", vb->as.native.type, vb);
  vb = (Value*)malloc(sizeof(Value)); *vb = v_native(rt_map_set, ty_func(NULL, (Type*[]){ty_map(NULL,ty_str(NULL),t_i), ty_str(NULL), t_i},3, t_u)); env_set(vm->global_env, "map-set", vb->as.native.type, vb);
  vb = (Value*)malloc(sizeof(Value)); *vb = v_native(rt_map_get, ty_func(NULL, (Type*[]){ty_map(NULL,ty_str(NULL),t_i), ty_str(NULL)},2, t_i)); env_set(vm->global_env, "map-get", vb->as.native.type, vb);
  vb = (Value*)malloc(sizeof(Value)); *vb = v_native(rt_map_len, ty_func(NULL, (Type*[]){ty_map(NULL,ty_str(NULL),t_i)},1, t_i)); env_set(vm->global_env, "map-len", vb->as.native.type, vb);
  // Option type operations
  Type *t_opt = ty_option(NULL, t_any);
  vb = (Value*)malloc(sizeof(Value)); *vb = v_native(rt_some, ty_func(NULL, (Type*[]){t_any},1, t_opt)); env_set(vm->global_env, "some", vb->as.native.type, vb);
  vb = (Value*)malloc(sizeof(Value)); *vb = v_native(rt_none_val, ty_func(NULL, (Type*[]){},0, t_opt)); env_set(vm->global_env, "none", vb->as.native.type, vb);
  vb = (Value*)malloc(sizeof(Value)); *vb = v_native(rt_is_some, ty_func(NULL, (Type*[]){t_opt},1, ty_bool(NULL))); env_set(vm->global_env, "some?", vb->as.native.type, vb);
  vb = (Value*)malloc(sizeof(Value)); *vb = v_native(rt_is_none, ty_func(NULL, (Type*[]){t_opt},1, ty_bool(NULL))); env_set(vm->global_env, "none?", vb->as.native.type, vb);
  vb = (Value*)malloc(sizeof(Value)); *vb = v_native(rt_unwrap, ty_func(NULL, (Type*[]){t_any},1, t_any)); env_set(vm->global_env, "unwrap", vb->as.native.type, vb);
  vb = (Value*)malloc(sizeof(Value)); *vb = v_native(rt_unwrap_or, ty_func(NULL, (Type*[]){t_any,t_any},2, t_any)); env_set(vm->global_env, "unwrap-or", vb->as.native.type, vb);
  // Result type operations
  Type *t_res = ty_result(NULL, t_any, t_any);
  vb = (Value*)malloc(sizeof(Value)); *vb = v_native(rt_ok_val, ty_func(NULL, (Type*[]){t_any},1, t_res)); env_set(vm->global_env, "ok", vb->as.native.type, vb);
  vb = (Value*)malloc(sizeof(Value)); *vb = v_native(rt_err_val, ty_func(NULL, (Type*[]){t_any},1, t_res)); env_set(vm->global_env, "err", vb->as.native.type, vb);
  vb = (Value*)malloc(sizeof(Value)); *vb = v_native(rt_is_ok, ty_func(NULL, (Type*[]){t_res},1, ty_bool(NULL))); env_set(vm->global_env, "ok?", vb->as.native.type, vb);
  vb = (Value*)malloc(sizeof(Value)); *vb = v_native(rt_is_err, ty_func(NULL, (Type*[]){t_res},1, ty_bool(NULL))); env_set(vm->global_env, "err?", vb->as.native.type, vb);
  vb = (Value*)malloc(sizeof(Value)); *vb = v_native(rt_unwrap_err, ty_func(NULL, (Type*[]){t_res},1, t_any)); env_set(vm->global_env, "unwrap-err", vb->as.native.type, vb);
  // Struct operations (struct-new is variadic, first arg is name string)
  vb = (Value*)malloc(sizeof(Value)); *vb = v_native(rt_struct_new, ty_func(NULL, (Type*[]){t_any},1, t_any)); env_set(vm->global_env, "struct-new", vb->as.native.type, vb);
  vb = (Value*)malloc(sizeof(Value)); *vb = v_native(rt_struct_get, ty_func(NULL, (Type*[]){t_any,t_i},2, t_any)); env_set(vm->global_env, "struct-get", vb->as.native.type, vb);
  vb = (Value*)malloc(sizeof(Value)); *vb = v_native(rt_struct_set, ty_func(NULL, (Type*[]){t_any,t_i,t_any},3, t_u)); env_set(vm->global_env, "struct-set", vb->as.native.type, vb);
  return vm;
}

void vm_free(VM *vm) {
  // Free module arenas
  ModArena *ma = vm->mod_arenas; while (ma) { ModArena *nx = ma->next; if (ma->arena) { arena_free(ma->arena); free(ma->arena); } free(ma); ma = nx; }
  // Free import cache
  ModNode *mn = vm->imported; while (mn) { ModNode *nx = mn->next; free(mn->path); free(mn); mn = nx; }
  gc_free_all(&vm->gc); env_free(vm->global_env); free(vm);
}

static int is_sym(Node *n, const char *s) {
  return n && n->kind==N_SYMBOL && strlen(s)==n->as.sym.len && strncmp(n->as.sym.ptr, s, n->as.sym.len)==0;
}

// no-op now; direct native fns are stored

// Closure call
typedef struct Closure Closure;
Value vm_call_closure(VM *vm, Closure *c, Value *args, int nargs);
Value vm_call_closure0(VM *vm, Closure *c) { return vm_call_closure(vm, c, NULL, 0); }

// Evaluate list as call or special form
static Value eval_list(VM *vm, Env *env, Node *list) {
  size_t n = list->as.list.count;
  if (n==0) return v_unit();
  Node *head = list->as.list.items[0];
  if (head->kind==N_SYMBOL) {
    if (is_sym(head, "defmacro")) {
      // handled in macro collection; during eval treat as no-op
      list->ty = ty_unit(NULL); return v_unit();
    }
    if (is_sym(head, "def")) {
      // [def name expr] or [def name : Type expr]
      if (n<3) return v_unit();
      Node *name = list->as.list.items[1];
      Node *expr = NULL; size_t idx=2;
      if (idx<n && list->as.list.items[idx]->kind==N_SYMBOL && is_sym(list->as.list.items[idx], ":")) {
        idx+=2; // skip ':' and type node
      }
      if (idx<n) expr = list->as.list.items[idx];
      Value v = eval_node(vm, env, expr);
      Value *box = (Value*)malloc(sizeof(Value)); *box = v;
      env_set(env, name->as.sym.ptr, expr?expr->ty:NULL, box);
      return v_unit();
    }
    if (is_sym(head, "quote")) {
      if (n<2) return v_unit();
      Node *q = list->as.list.items[1];
      // Turn AST node into Value (symbol/string/int/bool, and list)
      switch (q->kind) {
        case N_INT: return v_int(q->as.ival);
        case N_FLOAT: return v_float(q->as.fval);
        case N_BOOL: return v_bool(q->as.bval);
        case N_STRING: return v_str(rt_string_new(vm, q->as.str.ptr, q->as.str.len));
        case N_SYMBOL: return v_symbol(q->as.sym.ptr, (int32_t)q->as.sym.len);
        case N_LIST: {
          ValList *vl = (ValList*)gc_alloc(&vm->gc, sizeof(ValList), 3);
          vl->len = (int32_t)q->as.list.count; vl->cap = vl->len;
          vl->items = (Value*)malloc(sizeof(Value)*vl->len);
          for (int i=0;i<vl->len;i++) vl->items[i] = eval_node(vm, env, (Node*)q->as.list.items[i]);
          return v_list(vl);
        }
      }
    }
    if (is_sym(head, "quasiquote")) {
      // quasiquote with unquote and unquote-splicing
      if (n<2) { list->ty = ty_any(NULL); return v_unit(); }
      Node *q = list->as.list.items[1];
      // Helpers
      void vl_push(ValList *vl, Value v){ if (vl->len==vl->cap){ vl->cap=vl->cap?vl->cap*2:4; vl->items=(Value*)realloc(vl->items,sizeof(Value)*vl->cap);} vl->items[vl->len++]=v; }
      Value qq(Node *node) {
        if (node->kind==N_LIST) {
          // Handle unquote and unquote-splicing at head position
          if (node->as.list.count>=2 && node->as.list.items[0]->kind==N_SYMBOL && is_sym(node->as.list.items[0], "unquote")) {
            return eval_node(vm, env, node->as.list.items[1]);
          }
          ValList *vl = (ValList*)gc_alloc(&vm->gc, sizeof(ValList), 7);
          vl->len=0; vl->cap=0; vl->items=NULL;
          for (size_t i=0;i<node->as.list.count;i++) {
            Node *el = node->as.list.items[i];
            if (el->kind==N_LIST && el->as.list.count>=2 && el->as.list.items[0]->kind==N_SYMBOL && is_sym(el->as.list.items[0], "unquote-splicing")) {
              Value sp = eval_node(vm, env, el->as.list.items[1]);
              if (sp.kind==VAL_LIST && sp.as.list) {
                for (int k=0;k<sp.as.list->len;k++) vl_push(vl, sp.as.list->items[k]);
              } else {
                vl_push(vl, sp);
              }
            } else {
              vl_push(vl, qq(el));
            }
          }
          return v_list(vl);
        }
        switch (node->kind) {
          case N_INT: return v_int(node->as.ival);
          case N_FLOAT: return v_float(node->as.fval);
          case N_BOOL: return v_bool(node->as.bval);
          case N_STRING: return v_str(rt_string_new(vm, node->as.str.ptr, node->as.str.len));
          case N_SYMBOL: return v_symbol(node->as.sym.ptr, (int32_t)node->as.sym.len);
          default: return v_symbol("_",1);
        }
      }
      Value out = qq(q); list->ty = ty_any(NULL); return out;
    }
    if (is_sym(head, "let")) {
      // [let [[name expr] ...] body...]
      if (n<3) return v_unit();
      Node *binds = list->as.list.items[1];
      Env *child = env_new(env);
      for (size_t i=0;i<binds->as.list.count;i++) {
        Node *pair = binds->as.list.items[i];
        if (pair->kind!=N_LIST || pair->as.list.count<2) continue;
        Node *nm = pair->as.list.items[0];
        Node *ex = NULL;
        if (pair->as.list.count>=4 && is_sym(pair->as.list.items[1], ":")) {
          ex = pair->as.list.items[3];
        } else if (pair->as.list.count==3 && is_sym(pair->as.list.items[1], ":")) {
          // Next sibling list is the expr
          if (i+1<binds->as.list.count) { ex = binds->as.list.items[++i]; }
        } else {
          ex = pair->as.list.items[1];
        }
        Value v = eval_node(vm, child, ex);  // Use child env so previous bindings are visible
        Value *box = (Value*)malloc(sizeof(Value)); *box=v;
        env_set(child, nm->as.sym.ptr, ex->ty, box);
      }
      Value result = v_unit();
      for (size_t i=2;i<n;i++) result = eval_node(vm, child, list->as.list.items[i]);
      // NOTE: Don't free child - closures may have captured it (GC should handle this)
      // env_free(child);
      return result;
    }
    if (is_sym(head, "if")) {
      if (n!=4) return v_unit();
      Value cond = eval_node(vm, env, list->as.list.items[1]);
      if (cond.kind==VAL_BOOL && cond.as.b) return eval_node(vm, env, list->as.list.items[2]);
      return eval_node(vm, env, list->as.list.items[3]);
    }
    if (is_sym(head, "do")) {
      Value v=v_unit();
      for (size_t i=1;i<n;i++) v = eval_node(vm, env, list->as.list.items[i]);
      return v;
    }
    // while loop: [while condition body...]
    if (is_sym(head, "while")) {
      if (n < 2) return v_unit();
      Value result = v_unit();
      for (;;) {
        Value cond = eval_node(vm, env, list->as.list.items[1]);
        if (cond.kind != VAL_BOOL || !cond.as.b) break;
        for (size_t i = 2; i < n; i++) {
          result = eval_node(vm, env, list->as.list.items[i]);
        }
      }
      return result;
    }
    // set! mutation: [set! name value]
    if (is_sym(head, "set!")) {
      if (n != 3 || list->as.list.items[1]->kind != N_SYMBOL) return v_unit();
      const char *name = list->as.list.items[1]->as.sym.ptr;
      size_t namelen = list->as.list.items[1]->as.sym.len;
      char tmp[256]; if (namelen >= sizeof(tmp)) namelen = sizeof(tmp)-1;
      memcpy(tmp, name, namelen); tmp[namelen] = '\0';
      EnvEntry *e = env_lookup(env, tmp);
      if (!e || !e->value) { fprintf(stderr, "set!: undefined variable: %s\n", tmp); return v_unit(); }
      Value newval = eval_node(vm, env, list->as.list.items[2]);
      *(Value*)e->value = newval;
      return v_unit();
    }
    if (is_sym(head, "import")) {
      if (n!=2 || list->as.list.items[1]->kind!=N_STRING) return v_unit();
      char tmp[1024]; size_t len = list->as.list.items[1]->as.str.len;
      if (len >= sizeof(tmp)) len = sizeof(tmp)-1;
      memcpy(tmp, list->as.list.items[1]->as.str.ptr, len); tmp[len]='\0';
      (void)vm_import_resolve_and_load(vm, tmp);
      return v_unit();
    }
    // defstruct: [defstruct Name [[field1 : Type1] [field2 : Type2] ...]]
    if (is_sym(head, "defstruct")) {
      if (n < 3 || list->as.list.items[1]->kind != N_SYMBOL) return v_unit();
      const char *name = list->as.list.items[1]->as.sym.ptr;
      Node *fields_node = list->as.list.items[2];
      size_t nfields = fields_node->as.list.count;

      // Store type definition in environment
      Type **field_types = (Type**)malloc(sizeof(Type*) * nfields);
      const char **field_names = (const char**)malloc(sizeof(const char*) * nfields);
      for (size_t i = 0; i < nfields; i++) {
        Node *f = fields_node->as.list.items[i];
        field_names[i] = f->as.list.items[0]->as.sym.ptr;
        field_types[i] = (f->as.list.count >= 3) ? parse_type_node(f->as.list.items[2]) : ty_any(NULL);
      }
      Type *struct_ty = ty_struct(NULL, name, field_types, field_names, nfields);

      // Create constructor function that creates struct instances
      // Store type definition in env for lookup
      env_set(env, name, struct_ty, NULL);
      return v_unit();
    }
    // defenum: [defenum Name [Variant1 Variant2 ...]]
    if (is_sym(head, "defenum")) {
      if (n < 3 || list->as.list.items[1]->kind != N_SYMBOL) return v_unit();
      const char *name = list->as.list.items[1]->as.sym.ptr;
      Node *variants_node = list->as.list.items[2];
      size_t nvariants = variants_node->as.list.count;

      const char **variants = (const char**)malloc(sizeof(const char*) * nvariants);
      for (size_t i = 0; i < nvariants; i++) {
        variants[i] = variants_node->as.list.items[i]->as.sym.ptr;
      }
      Type *enum_ty = ty_enum(NULL, name, variants, nvariants);

      // Register enum type and variant constructors
      env_set(env, name, enum_ty, NULL);

      // Register each variant as a value
      for (size_t i = 0; i < nvariants; i++) {
        Value *vb = (Value*)malloc(sizeof(Value));
        *vb = v_int((int64_t)i); // Each variant is an integer tag
        env_set(env, variants[i], ty_int(NULL), vb);
      }
      return v_unit();
    }
    if (is_sym(head, "fn")) {
      // Build closure
      Closure *c = (Closure*)gc_alloc(&vm->gc, sizeof(Closure), 2);
      c->fn_node = list; c->env = env; c->type = list->ty; // static type annotated
      return v_closure(c);
    }
  }
  // Function call
  // Evaluate head
  Value fval = eval_node(vm, env, head);
  int argc = (int)(n-1);
  Value *argv = (Value*)alloca(sizeof(Value)*argc);
  for (int i=0;i<argc;i++) argv[i] = eval_node(vm, env, list->as.list.items[i+1]);
  if (fval.kind==VAL_FUNC) return fval.as.native.fn(env, argv, argc);
  if (fval.kind==VAL_CLOSURE) return vm_call_closure(vm, fval.as.clos, argv, argc);
  return v_unit();
}

static Value eval_node(VM *vm, Env *env, Node *n) {
  switch (n->kind) {
    case N_INT: return v_int(n->as.ival);
    case N_FLOAT: return v_float(n->as.fval);
    case N_BOOL: return v_bool(n->as.bval);
    case N_STRING: return v_str(rt_string_new(vm, n->as.str.ptr, n->as.str.len));
    case N_SYMBOL: {
      EnvEntry *e = env_lookup(env, n->as.sym.ptr);
      if (!e) return v_unit();
      if (!e->value) return v_unit();
      return *(Value*)e->value;
    }
    case N_LIST: return eval_list(vm, env, n);
  }
  return v_unit();
}

Value vm_call_closure(VM *vm, Closure *c, Value *args, int nargs) {
  // fn form: [fn [[name : Type] ...] : Ret body...]
  Node *fn = (Node*)c->fn_node; Node *params = fn->as.list.items[1];
  Env *callenv = env_new(c->env);
  Value result = v_unit();
  int provided = nargs;
  int expected = (int)params->as.list.count;
  int nbind = provided<expected?provided:expected;
  for (int i=0;i<nbind;i++) {
    Node *p = params->as.list.items[i];
    const char *nm = p->as.list.items[0]->as.sym.ptr;
    Value *box = (Value*)malloc(sizeof(Value)); *box = args[i];
    env_set(callenv, nm, NULL, box);
  }
  // Execute body
  size_t i0 = 2; // skip 'fn' and params
  // optional ':' ret-type
  if (fn->as.list.count>i0 && is_sym(fn->as.list.items[i0], ":")) i0+=2;
  for (size_t i=i0;i<fn->as.list.count;i++) result = eval_node(vm, callenv, fn->as.list.items[i]);
  // NOTE: Don't free callenv - closures may have captured it (GC should handle this)
  // env_free(callenv);
  return result;
}

// Type parsing for primitive and function types with syntax: [T1 T2 -> R]
static Type *parse_type_node(Node *n) {
  if (!n) return ty_error(NULL);
  if (n->kind==N_SYMBOL) {
    if (strncmp(n->as.sym.ptr, "Int", n->as.sym.len)==0) return ty_int(NULL);
    if (strncmp(n->as.sym.ptr, "Float", n->as.sym.len)==0) return ty_float(NULL);
    if (strncmp(n->as.sym.ptr, "Bool", n->as.sym.len)==0) return ty_bool(NULL);
    if (strncmp(n->as.sym.ptr, "Str", n->as.sym.len)==0) return ty_str(NULL);
    if (strncmp(n->as.sym.ptr, "Unit", n->as.sym.len)==0) return ty_unit(NULL);
    if (strncmp(n->as.sym.ptr, "Any", n->as.sym.len)==0) return ty_any(NULL);
  }
  if (n->kind==N_LIST) {
    // Chan
    if (n->as.list.count==2 && n->as.list.items[0]->kind==N_SYMBOL && is_sym(n->as.list.items[0], "Chan")) {
      return ty_chan(NULL, parse_type_node(n->as.list.items[1]));
    }
    // Vec
    if (n->as.list.count==2 && n->as.list.items[0]->kind==N_SYMBOL && is_sym(n->as.list.items[0], "Vec")) {
      return ty_vec(NULL, parse_type_node(n->as.list.items[1]));
    }
    // Map
    if (n->as.list.count==3 && n->as.list.items[0]->kind==N_SYMBOL && is_sym(n->as.list.items[0], "Map")) {
      return ty_map(NULL, parse_type_node(n->as.list.items[1]), parse_type_node(n->as.list.items[2]));
    }
    // Func
    for (size_t i=0;i<n->as.list.count;i++) if (n->as.list.items[i]->kind==N_SYMBOL && is_sym(n->as.list.items[i], "->")) {
      size_t arrow = i; size_t arity = arrow;
      Type **params = (Type**)malloc(sizeof(Type*)*arity);
      for (size_t j=0;j<arrow;j++) params[j] = parse_type_node(n->as.list.items[j]);
      Type *ret = parse_type_node(n->as.list.items[arrow+1]);
      return ty_func(NULL, params, arity, ret);
    }
  }
  return ty_error(NULL);
}

// Minimal typechecker: annotate nodes with types; assumes correct programs (explicit annotations)
static int typecheck_list(Env *tenv, Node *list);
static int typecheck_node(Env *tenv, Node *n) {
  switch (n->kind) {
    case N_INT: n->ty = ty_int(NULL); return 1;
    case N_FLOAT: n->ty = ty_float(NULL); return 1;
    case N_BOOL: n->ty = ty_bool(NULL); return 1;
    case N_STRING: n->ty = ty_str(NULL); return 1;
    case N_SYMBOL: {
      EnvEntry *e = env_lookup(tenv, n->as.sym.ptr);
      n->ty = e? e->type : ty_error(NULL); return e!=NULL;
    }
    case N_LIST: return typecheck_list(tenv, n);
  }
  return 0;
}

static int typecheck_list(Env *tenv, Node *list) {
  if (list->as.list.count==0) { list->ty=ty_unit(NULL); return 1; }
  Node *head = list->as.list.items[0];
  if (head->kind==N_SYMBOL) {
    if (is_sym(head, "defmacro")) { list->ty = ty_unit(NULL); return 1; }
    if (is_sym(head, "def")) {
      // [def name : Type expr]
      if (list->as.list.count<5) { fprintf(stderr, "def: too few items (%zu)\n", list->as.list.count); return 0; }
      const char *name = list->as.list.items[1]->as.sym.ptr;
      Node *type_node = list->as.list.items[3];
      Type *decl = parse_type_node(type_node);
      if (!decl || decl->kind==TY_ERROR) { fprintf(stderr, "def: bad type\n"); return 0; }
      Env *child = tenv; // def binds in tenv
      env_set(child, name, decl, NULL);
      Node *expr = list->as.list.items[4];
      if (!typecheck_node(tenv, expr)) { fprintf(stderr, "def: expr type error\n"); return 0; }
      if (!ty_eq(expr->ty, decl)) { fprintf(stderr, "def: type mismatch\n"); return 0; }
      list->ty = ty_unit(NULL); return 1;
    }
    if (is_sym(head, "fn")) {
      // [fn [[name : T] ...] : R body...]
      Node *params = list->as.list.items[1];
      size_t i = 2; Type *ret = ty_unit(NULL);
      Env *child = env_new(tenv);
      if (list->as.list.count>i && is_sym(list->as.list.items[i], ":")) {
        ret = parse_type_node(list->as.list.items[i+1]); i+=2;
      }
      size_t arity = params->as.list.count;
      Type **pt = (Type**)malloc(sizeof(Type*)*arity);
      for (size_t k=0;k<arity;k++) {
        Node *p = params->as.list.items[k];
        const char *nm = p->as.list.items[0]->as.sym.ptr;
        Type *ty = parse_type_node(p->as.list.items[2]);
        pt[k]=ty; env_set(child, nm, ty, NULL);
      }
      // Check body; result is last expr
      for (; i<list->as.list.count-1; i++) if (!typecheck_node(child, list->as.list.items[i])) return 0;
      if (!typecheck_node(child, list->as.list.items[list->as.list.count-1])) return 0;
      if (!ty_eq(list->as.list.items[list->as.list.count-1]->ty, ret)) return 0;
      list->ty = ty_func(NULL, pt, arity, ret); env_free(child); return 1;
    }
    if (is_sym(head, "quote")) { list->ty = ty_any(NULL); return 1; }
    if (is_sym(head, "quasiquote")) { list->ty = ty_any(NULL); return 1; }
    if (is_sym(head, "do")) {
      // Sequence; type is last item's type or Unit
      Type *t = ty_unit(NULL);
      for (size_t i=1;i<list->as.list.count;i++) {
        if (!typecheck_node(tenv, list->as.list.items[i])) return 0;
        t = list->as.list.items[i]->ty;
      }
      list->ty = t; return 1;
    }
    if (is_sym(head, "import")) {
      // [import "module"]
      if (list->as.list.count!=2) return 0;
      if (list->as.list.items[1]->kind!=N_STRING) return 0;
      list->ty = ty_unit(NULL); return 1;
    }
    if (is_sym(head, "let")) {
      Node *bindings = list->as.list.items[1];
      Env *child = env_new(tenv);
      for (size_t i2=0;i2<bindings->as.list.count;i2++) {
        Node *b=bindings->as.list.items[i2];
        const char *nm=b->as.list.items[0]->as.sym.ptr;
        Node *expr = NULL; Type *ty = NULL;
        if (b->as.list.count>=4 && is_sym(b->as.list.items[1], ":")) {
          ty = parse_type_node(b->as.list.items[2]); expr = b->as.list.items[3];
        } else if (b->as.list.count==3 && is_sym(b->as.list.items[1], ":")) {
          ty = parse_type_node(b->as.list.items[2]);
          if (i2+1<bindings->as.list.count) { expr = bindings->as.list.items[++i2]; }
        } else {
          expr = b->as.list.items[1];
        }
        if (!expr) { env_free(child); return 0; }
        if (!typecheck_node(child, expr)) { env_free(child); return 0; }
        if (!ty) ty = expr->ty;
        env_set(child, nm, ty, NULL);
        if (!ty_eq(expr->ty, ty)) { env_free(child); return 0; }
      }
      int ok=1;
      for (size_t i=2;i<list->as.list.count && ok;i++) ok = typecheck_node(child, list->as.list.items[i]);
      list->ty = list->as.list.count>2 ? list->as.list.items[list->as.list.count-1]->ty : ty_unit(NULL);
      env_free(child); return ok;
    }
    if (is_sym(head, "if")) {
      if (!typecheck_node(tenv, list->as.list.items[1])) return 0;
      if (!typecheck_node(tenv, list->as.list.items[2])) return 0;
      if (!typecheck_node(tenv, list->as.list.items[3])) return 0;
      if (!ty_eq(list->as.list.items[2]->ty, list->as.list.items[3]->ty)) return 0;
      list->ty = list->as.list.items[2]->ty; return 1;
    }
    // while: condition must be Bool, body returns Unit
    if (is_sym(head, "while")) {
      if (list->as.list.count < 2) return 0;
      if (!typecheck_node(tenv, list->as.list.items[1])) return 0;
      if (!list->as.list.items[1]->ty || list->as.list.items[1]->ty->kind != TY_BOOL) return 0;
      for (size_t i = 2; i < list->as.list.count; i++) {
        if (!typecheck_node(tenv, list->as.list.items[i])) return 0;
      }
      list->ty = ty_unit(NULL); return 1;
    }
    // set!: lookup variable and check type matches
    if (is_sym(head, "set!")) {
      if (list->as.list.count != 3 || list->as.list.items[1]->kind != N_SYMBOL) return 0;
      char tmp[256]; size_t len = list->as.list.items[1]->as.sym.len;
      if (len >= sizeof(tmp)) len = sizeof(tmp)-1;
      memcpy(tmp, list->as.list.items[1]->as.sym.ptr, len); tmp[len] = '\0';
      EnvEntry *e = env_lookup(tenv, tmp);
      if (!e) { fprintf(stderr, "set!: undefined variable: %s\n", tmp); return 0; }
      if (!typecheck_node(tenv, list->as.list.items[2])) return 0;
      // Type must match (or be compatible with Any)
      if (e->type && !ty_eq(e->type, list->as.list.items[2]->ty)) return 0;
      list->ty = ty_unit(NULL); return 1;
    }
    if (is_sym(head, "vec")) {
      // variadic; element types may differ; return Vec Any
      for (size_t i=1;i<list->as.list.count;i++) if (!typecheck_node(tenv, list->as.list.items[i])) return 0;
      list->ty = ty_vec(NULL, ty_any(NULL)); return 1;
    }
    // struct-new: variadic, first arg is type name string, rest are field values
    if (is_sym(head, "struct-new")) {
      for (size_t i=1;i<list->as.list.count;i++) if (!typecheck_node(tenv, list->as.list.items[i])) return 0;
      list->ty = ty_any(NULL); return 1;
    }
    // defstruct: [defstruct Name [[field1 : Type1] ...]]
    if (is_sym(head, "defstruct")) {
      if (list->as.list.count < 3 || list->as.list.items[1]->kind != N_SYMBOL) return 0;
      const char *name = list->as.list.items[1]->as.sym.ptr;
      Node *fields_node = list->as.list.items[2];
      size_t nfields = fields_node->as.list.count;
      Type **field_types = (Type**)malloc(sizeof(Type*) * nfields);
      const char **field_names = (const char**)malloc(sizeof(const char*) * nfields);
      for (size_t i = 0; i < nfields; i++) {
        Node *f = fields_node->as.list.items[i];
        field_names[i] = f->as.list.items[0]->as.sym.ptr;
        field_types[i] = (f->as.list.count >= 3) ? parse_type_node(f->as.list.items[2]) : ty_any(NULL);
      }
      Type *struct_ty = ty_struct(NULL, name, field_types, field_names, nfields);
      env_set(tenv, name, struct_ty, NULL);
      list->ty = ty_unit(NULL); return 1;
    }
    // defenum: [defenum Name [Variant1 Variant2 ...]]
    if (is_sym(head, "defenum")) {
      if (list->as.list.count < 3 || list->as.list.items[1]->kind != N_SYMBOL) return 0;
      const char *name = list->as.list.items[1]->as.sym.ptr;
      Node *variants_node = list->as.list.items[2];
      size_t nvariants = variants_node->as.list.count;
      const char **variants = (const char**)malloc(sizeof(const char*) * nvariants);
      for (size_t i = 0; i < nvariants; i++) {
        variants[i] = variants_node->as.list.items[i]->as.sym.ptr;
      }
      Type *enum_ty = ty_enum(NULL, name, variants, nvariants);
      env_set(tenv, name, enum_ty, NULL);
      // Register each variant as an Int
      for (size_t i = 0; i < nvariants; i++) {
        env_set(tenv, variants[i], ty_int(NULL), NULL);
      }
      list->ty = ty_unit(NULL); return 1;
    }
  }
  // Call typechecking: head must have function type in env
  if (!typecheck_node(tenv, head)) return 0;
  if (!head->ty || head->ty->kind!=TY_FUNC) return 0;
  Type *fty = head->ty;
  if (fty->as.fn.arity != list->as.list.count-1) return 0;
  for (size_t i=0;i<fty->as.fn.arity;i++) {
    if (!typecheck_node(tenv, list->as.list.items[i+1])) return 0;
    if (!ty_eq(fty->as.fn.params[i], list->as.list.items[i+1]->ty)) return 0;
  }
  list->ty = fty->as.fn.ret; return 1;
}

int eval_program(VM *vm, Node *program) {
  // Typecheck each toplevel form, then evaluate
  for (size_t i=0;i<program->as.list.count;i++) {
    Node *form = program->as.list.items[i];
    // Handle import eagerly to populate env
    if (form->kind==N_LIST && form->as.list.count==2 && form->as.list.items[0]->kind==N_SYMBOL && is_sym(form->as.list.items[0], "import") && form->as.list.items[1]->kind==N_STRING) {
      char tmp[1024]; size_t len=form->as.list.items[1]->as.str.len; if (len>=sizeof(tmp)) len=sizeof(tmp)-1; memcpy(tmp, form->as.list.items[1]->as.str.ptr, len); tmp[len]='\0';
      (void)vm_import_resolve_and_load(vm, tmp);
      continue;
    }
    if (!typecheck_node(vm->global_env, form)) {
      if (form->kind==N_LIST && form->as.list.count>0 && form->as.list.items[0]->kind==N_SYMBOL) {
        fprintf(stderr, "Type error in form %zu starting with '%.*s'\n", i,
          (int)form->as.list.items[0]->as.sym.len, form->as.list.items[0]->as.sym.ptr);
      } else {
        fprintf(stderr, "Type error in toplevel form %zu.\n", i);
      }
      return 1;
    }
  }
  for (size_t i=0;i<program->as.list.count;i++) {
    (void)eval_node(vm, vm->global_env, program->as.list.items[i]);
  }
  return 0;
}

int eval_form(VM *vm, Node *form, Value *out) {
  if (!typecheck_node(vm->global_env, form)) return 1;
  *out = eval_node(vm, vm->global_env, form);
  return 0;
}

// Called from runtime spawn
void vm_call_closure_noargs(struct VM *vm, struct Closure *c) { (void)vm_call_closure(vm, c, NULL, 0); }

// File import helper
static char *read_file_all2(const char *path, size_t *out_len) {
  FILE *f = fopen(path, "rb"); if (!f) return NULL;
  fseek(f, 0, SEEK_END); long n = ftell(f); fseek(f, 0, SEEK_SET);
  char *buf = (char*)malloc(n+1); if (!buf){ fclose(f); return NULL; }
  size_t r = fread(buf,1,n,f); fclose(f); buf[r]='\0'; if(out_len)*out_len=r; return buf;
}

static int vm_import_file_impl(VM *vm, const char *path) {
  size_t n=0; char *buf = read_file_all2(path, &n); if (!buf) return 1;
  Arena *arena = (Arena*)malloc(sizeof(Arena)); arena_init(arena, 1<<20);
  Parser p; parser_init(&p, arena, buf, n);
  Node *prog = parse_toplevel(&p);
  int rc = eval_program(vm, prog);
  // retain arena for lifetime of VM
  ModArena *ma = (ModArena*)malloc(sizeof(ModArena)); ma->arena = arena; ma->next = vm->mod_arenas; vm->mod_arenas = ma;
  free(buf); return rc;
}

int vm_import_file(VM *vm, const char *path) { return vm_import_file_impl(vm, path); }

static int file_exists(const char *p){ FILE *f=fopen(p,"rb"); if(!f) return 0; fclose(f); return 1; }
static void replace_char(char *s, char a, char b){ for (;*s;s++) if (*s==a) *s=b; }
static int already_imported(VM *vm, const char *path){ for (ModNode *m=vm->imported;m;m=m->next) if (strcmp(m->path,path)==0) return 1; return 0; }
static char *dupstr(const char *s){ size_t n=strlen(s); char *p=(char*)malloc(n+1); memcpy(p,s,n); p[n]='\0'; return p; }
static void mark_imported(VM *vm, const char *path){ ModNode *m=(ModNode*)malloc(sizeof(ModNode)); m->path=dupstr(path); m->next=vm->imported; vm->imported=m; }

static int vm_import_resolve_and_load(VM *vm, const char *name){
  char candidate[1024];
  // Direct path
  if (strstr(name, "/") || strstr(name, ".sq")) {
    snprintf(candidate,sizeof(candidate),"%s",name);
    if (!already_imported(vm, candidate) && file_exists(candidate)) { mark_imported(vm, candidate); return vm_import_file_impl(vm, candidate);} }
  // Module style: dots -> slashes + .sq
  char mod[512]; size_t nl=strlen(name); if (nl>=sizeof(mod)) nl=sizeof(mod)-1; memcpy(mod,name,nl); mod[nl]='\0'; replace_char(mod,'.','/');
  const char *bases[] = { "./", "packages/", "std/", "sqale/packages/", "sqale/std/", NULL };
  for (int i=0;bases[i];i++) {
    snprintf(candidate,sizeof(candidate),"%s%s.sq", bases[i], mod);
    if (file_exists(candidate) && !already_imported(vm, candidate)) { mark_imported(vm, candidate); return vm_import_file_impl(vm, candidate); }
  }
  // Env var SQALE_PATH (colon-separated)
  const char *sp = getenv("SQALE_PATH");
  if (sp) {
    char buf[2048]; size_t sl=strlen(sp); if (sl>=sizeof(buf)) sl=sizeof(buf)-1; memcpy(buf,sp,sl); buf[sl]='\0';
    char *tok=strtok(buf, ":");
    while (tok) {
      snprintf(candidate,sizeof(candidate),"%s/%s.sq",tok,mod);
      if (file_exists(candidate) && !already_imported(vm, candidate)) { mark_imported(vm, candidate); return vm_import_file_impl(vm, candidate); }
      tok=strtok(NULL, ":");
    }
  }
  // Not found
  fprintf(stderr, "import: module not found: %s\n", name);
  return 1;
}
