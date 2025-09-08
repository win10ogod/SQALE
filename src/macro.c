#include "macro.h"
#include "eval.h"
#include "value.h"
#include "runtime.h"
#include "gc.h"
#include <string.h>
#include <stdlib.h>
#include <alloca.h>

// static helper reserved; currently unused

void macro_env_add(MacroEnv **env, const char *name, MacroFn fn) {
  MacroEnv *m = (MacroEnv*)malloc(sizeof(MacroEnv));
  m->name=name; m->is_closure=0; m->fn=fn; m->next=*env; *env=m;
}
void macro_env_add_closure(MacroEnv **env, const char *name, struct VM *vm, struct Closure *c) {
  MacroEnv *m = (MacroEnv*)malloc(sizeof(MacroEnv));
  m->name=name; m->is_closure=1; m->fn=NULL; m->clos.vm=vm; m->clos.c=c; m->next=*env; *env=m;
}

static MacroEnv *lookup(MacroEnv *env, Node *head) {
  if (!head || head->kind!=N_SYMBOL) return NULL;
  for (MacroEnv *m=env;m;m=m->next) if (strncmp(head->as.sym.ptr, m->name, head->as.sym.len)==0 && m->name[head->as.sym.len]=='\0') return m;
  return NULL;
}

static Node *expand_rec(Arena *a, MacroEnv *env, Node *n, int inside_type);

static int list_looks_like_type(Node *lst) {
  for (size_t i=0;i<lst->as.list.count;i++) {
    Node *it = lst->as.list.items[i];
    if (it->kind==N_SYMBOL && ((it->as.sym.len==2 && strncmp(it->as.sym.ptr, "->",2)==0) || (it->as.sym.len==4 && strncmp(it->as.sym.ptr,"Chan",4)==0))) return 1;
  }
  return 0;
}

static Node *expand_list(Arena *a, MacroEnv *env, Node *lst) {
  if (lst->as.list.count==0) return lst;
  int is_type_context = list_looks_like_type(lst);
  // Do not expand macros inside type lists
  if (is_type_context) return lst;
  Node *head = lst->as.list.items[0];
  MacroEnv *me = lookup(env, head);
  if (me) {
    Node *out = NULL;
    if (!me->is_closure) {
      out = me->fn(a, lst);
    } else {
      // Convert args to AST values and invoke closure
      extern Value vm_call_closure(struct VM*, struct Closure*, Value*, int);
      extern Value v_symbol(const char*, int32_t);
      extern Value v_list(ValList*);
      extern Value v_int(int64_t);
      extern Value v_float(double);
      extern Value v_bool(bool);
      extern Value v_str(String*);
      extern String *rt_string_new(struct VM*, const char*, size_t);

      // node->value (AST) converter
      Value node_to_val(Node *n) {
        switch (n->kind) {
          case N_SYMBOL: return v_symbol(n->as.sym.ptr, (int32_t)n->as.sym.len);
          case N_INT: return v_int(n->as.ival);
          case N_FLOAT: return v_float(n->as.fval);
          case N_BOOL: return v_bool(n->as.bval);
          case N_STRING: return v_str(rt_string_new(me->clos.vm, n->as.str.ptr, n->as.str.len));
          case N_LIST: {
            ValList *vl = (ValList*)gc_alloc(&me->clos.vm->gc, sizeof(ValList), 6);
            vl->len = (int32_t)n->as.list.count; vl->cap = vl->len; vl->items = (Value*)malloc(sizeof(Value)*vl->len);
            for (int i=0;i<vl->len;i++) vl->items[i] = node_to_val(n->as.list.items[i]);
            return v_list(vl);
          }
        }
        return v_symbol("_",1);
      }
      // value->node converter
      Node *val_to_node(Value v) {
        switch (v.kind) {
          case VAL_INT: return node_new_int(a, v.as.i, 0,0);
          case VAL_FLOAT: return node_new_float(a, v.as.f, 0,0);
          case VAL_BOOL: return node_new_bool(a, v.as.b, 0,0);
          case VAL_STR: return node_new_string(a, v.as.str->data, (size_t)v.as.str->len, 0,0);
          case VAL_SYMBOL: return node_new_symbol(a, v.as.sym.name, (size_t)v.as.sym.len, 0,0);
          case VAL_LIST: {
            Node *nl = node_new_list(a, (size_t)v.as.list->len);
            for (int i=0;i<v.as.list->len;i++) node_list_push(a, nl, val_to_node(v.as.list->items[i]));
            return nl;
          }
          default: return node_new_symbol(a, "_",1,0,0);
        }
      }
      int argc = (int)(lst->as.list.count-1);
      Value *argv = (Value*)alloca(sizeof(Value)*argc);
      for (int i=0;i<argc;i++) argv[i] = node_to_val(lst->as.list.items[i+1]);
      Value res = vm_call_closure(me->clos.vm, me->clos.c, argv, argc);
      out = val_to_node(res);
    }
    return expand_rec(a, env, out, 0);
  }
  // otherwise expand children
  Node *nl = node_new_list(a, lst->as.list.count);
  for (size_t i=0;i<lst->as.list.count;i++) {
    int child_in_type = 0;
    // If a preceding ':' marker exists, mark types after it
    for (size_t j=0;j<i;j++) if (lst->as.list.items[j]->kind==N_SYMBOL && lst->as.list.items[j]->as.sym.len==1 && lst->as.list.items[j]->as.sym.ptr[0]==':') { child_in_type=1; break; }
    node_list_push(a, nl, expand_rec(a, env, lst->as.list.items[i], child_in_type));
  }
  return nl;
}

static Node *expand_rec(Arena *a, MacroEnv *env, Node *n, int inside_type) {
  if (!n) return n;
  if (inside_type) return n;
  if (n->kind==N_LIST) return expand_list(a, env, n);
  return n;
}

Node *macro_expand_all(Arena *a, MacroEnv *env, Node *n) { return expand_rec(a, env, n, 0); }

// -------- Built-in macros ---------

// [when test body...] => [if test [do body...] [do]]
static Node *m_when(Arena *a, Node *form) {
  if (form->as.list.count<2) return form;
  Node *test = form->as.list.items[1];
  Node *then_do = node_new_list(a, form->as.list.count-2+1);
  node_list_push(a, then_do, node_new_symbol(a, "do", 2, 0, 0));
  for (size_t i=2;i<form->as.list.count;i++) node_list_push(a, then_do, form->as.list.items[i]);
  Node *else_do = node_new_list(a, 1);
  node_list_push(a, else_do, node_new_symbol(a, "do", 2, 0, 0));
  Node *out = node_new_list(a, 4);
  node_list_push(a, out, node_new_symbol(a, "if", 2, 0, 0));
  node_list_push(a, out, test);
  node_list_push(a, out, then_do);
  node_list_push(a, out, else_do);
  return out;
}

// [cond [t a...] [t2 b...] [else e...]] => nested ifs
static Node *m_cond(Arena *a, Node *form) {
  size_t n = form->as.list.count;
  if (n<2) return form;
  // Build from last clause backwards
  Node *acc = node_new_list(a, 1);
  node_list_push(a, acc, node_new_symbol(a, "do", 2, 0, 0));
  for (size_t i=n-1;i>=1;i--) {
    Node *cl = form->as.list.items[i];
    if (cl->kind!=N_LIST || cl->as.list.count==0) continue;
    Node *first = cl->as.list.items[0];
    Node *body = node_new_list(a, cl->as.list.count);
    node_list_push(a, body, node_new_symbol(a, "do", 2, 0, 0));
    for (size_t j=1;j<cl->as.list.count;j++) node_list_push(a, body, cl->as.list.items[j]);
    if (first->kind==N_SYMBOL && strncmp(first->as.sym.ptr, "else", first->as.sym.len)==0 && first->as.sym.len==4) {
      acc = body; // else body
    } else {
      Node *iff = node_new_list(a, 4);
      node_list_push(a, iff, node_new_symbol(a, "if", 2, 0, 0));
      node_list_push(a, iff, first);
      node_list_push(a, iff, body);
      node_list_push(a, iff, acc);
      acc = iff;
    }
    if (i==1) break; // prevent size_t underflow
  }
  return acc;
}

// [-> x f g [h a b]] => [h [g [f x]] a b]
static Node *m_thread(Arena *a, Node *form) {
  if (form->as.list.count<2) return form;
  Node *x = form->as.list.items[1];
  Node *acc = x;
  for (size_t i=2;i<form->as.list.count;i++) {
    Node *step = form->as.list.items[i];
    if (step->kind==N_SYMBOL) {
      Node *call = node_new_list(a, 2);
      node_list_push(a, call, step);
      node_list_push(a, call, acc);
      acc = call;
    } else if (step->kind==N_LIST && step->as.list.count>=1) {
      Node *call = node_new_list(a, step->as.list.count+1);
      for (size_t j=0;j<step->as.list.count;j++) node_list_push(a, call, step->as.list.items[j]);
      // insert acc as second element (first arg)
      // shift by rebuilding
      Node *call2 = node_new_list(a, call->as.list.count+1);
      node_list_push(a, call2, call->as.list.items[0]);
      node_list_push(a, call2, acc);
      for (size_t j=1;j<call->as.list.count;j++) node_list_push(a, call2, call->as.list.items[j]);
      acc = call2;
    }
  }
  return acc;
}

void macros_register_core(MacroEnv **env) {
  macro_env_add(env, "when", m_when);
  macro_env_add(env, "cond", m_cond);
  macro_env_add(env, "->", m_thread);
}

// Collect user macros: [defmacro name [params...] body]
static int is_sym(Node *n, const char *s) {
  return n && n->kind==N_SYMBOL && strlen(s)==n->as.sym.len && strncmp(n->as.sym.ptr, s, n->as.sym.len)==0;
}

void macros_collect_user(Arena *a, MacroEnv **env, struct VM *vm, Node *program) {
  (void)a;
  for (size_t i=0;i<program->as.list.count;i++) {
    Node *f = program->as.list.items[i];
    if (f->kind!=N_LIST || f->as.list.count<3) continue;
    if (!is_sym(f->as.list.items[0], "defmacro")) continue;
    Node *name = f->as.list.items[1];
    Node *params = f->as.list.items[2];
    Node *body = f->as.list.items[3];
    // Build a typed fn: [fn [[p1 : Any] ...] : Any body]
    Node *fn = node_new_list(a, 4);
    node_list_push(a, fn, node_new_symbol(a, "fn", 2,0,0));
    Node *plist = node_new_list(a, params->as.list.count);
    for (size_t j=0;j<params->as.list.count;j++) {
      Node *pslot = node_new_list(a, 3);
      Node *pname = params->as.list.items[j];
      node_list_push(a, pslot, pname);
      node_list_push(a, pslot, node_new_symbol(a, ":",1,0,0));
      node_list_push(a, pslot, node_new_symbol(a, "Any",3,0,0));
      node_list_push(a, plist, pslot);
    }
    node_list_push(a, fn, plist);
    node_list_push(a, fn, node_new_symbol(a, ":",1,0,0));
    node_list_push(a, fn, node_new_symbol(a, "Any",3,0,0));
    // Append body
    node_list_push(a, fn, body);
    // Evaluate fn to closure in macro-time VM and register
    extern int eval_form(struct VM*, Node*, Value*);
    Value closv; if (eval_form(vm, fn, &closv)==0 && closv.kind==VAL_CLOSURE) {
      macro_env_add_closure(env, name->as.sym.ptr, vm, closv.as.clos);
    }
  }
}
