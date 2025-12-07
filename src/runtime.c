#include "runtime.h"
#include "gc.h"
#include "thread.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// Forward from eval.c
struct VM; struct Closure; 
void vm_call_closure_noargs(struct VM *vm, struct Closure *c);

String *rt_string_new(VM *vm, const char *bytes, size_t len) {
  (void)vm; // GC embedded in obj header; simple malloc for char data
  String *s = (String*)gc_alloc(&vm->gc, sizeof(String), 1);
  s->len = (int64_t)len;
  s->data = (char*)malloc(len+1);
  memcpy(s->data, bytes, len); s->data[len]='\0';
  return s;
}

String *rt_string_from_cstr(VM *vm, const char *cstr) {
  return rt_string_new(vm, cstr, strlen(cstr));
}

static int expect_nargs(int got, int expected, const char *name) {
  if (got != expected) {
    fprintf(stderr, "%s: expected %d args, got %d\n", name, expected, got);
    return 0;
  }
  return 1;
}

Value rt_print(Env *env, Value *args, int nargs) {
  (void)env;
  for (int i=0;i<nargs;i++) {
    Value v = args[i];
    switch (v.kind) {
      case VAL_INT: printf("%lld", (long long)v.as.i); break;
      case VAL_FLOAT: printf("%g", v.as.f); break;
      case VAL_BOOL: printf(v.as.b?"true":"false"); break;
      case VAL_STR: printf("%.*s", (int)v.as.str->len, v.as.str->data); break;
      case VAL_VEC: {
        printf("[");
        for (int j=0;j<v.as.vec->len;j++) {
          Value e = v.as.vec->items[j];
          if (e.kind==VAL_INT) printf("%lld", (long long)e.as.i);
          else if (e.kind==VAL_STR) printf("\"%.*s\"", (int)e.as.str->len, e.as.str->data);
          else if (e.kind==VAL_BOOL) printf(e.as.b?"true":"false");
          else printf("_");
          if (j+1<v.as.vec->len) printf(" ");
        }
        printf("]");
        break;
      }
      case VAL_UNIT: printf("()"); break;
      default: printf("<val>"); break;
    }
    if (i+1<nargs) printf(" ");
  }
  printf("\n");
  return v_unit();
}

// ============================================================================
// LLVM/run-time shims for codegen
// These functions are called from LLVM-generated code
// ============================================================================

void sq_print_i64(long long v) { printf("%lld", v); }
void sq_print_f64(double v) { printf("%g", v); }
void sq_print_bool(int v) { printf(v ? "true" : "false"); }
void sq_print_cstr(const char *s) { if (s) printf("%s", s); }
void sq_print_newline(void) { printf("\n"); }

// Runtime memory allocation for LLVM codegen
void *sq_alloc(size_t size) {
  return calloc(1, size);
}

// Closure allocation: stores function pointer, environment, and arity
typedef struct {
  void *fn;
  void *env;
  int arity;
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

static char *read_file_all(const char *path, size_t *out_len) {
  FILE *f = fopen(path, "rb"); if (!f) return NULL;
  fseek(f, 0, SEEK_END); long n = ftell(f); fseek(f, 0, SEEK_SET);
  char *buf = (char*)malloc(n+1); if (!buf){ fclose(f); return NULL; }
  size_t r = fread(buf,1,n,f); fclose(f); buf[r]='\0'; if(out_len)*out_len=r; return buf;
}

Value rt_read_file(Env *env, Value *args, int nargs) {
  (void)env; if (!expect_nargs(nargs,1,"read-file")) return v_unit();
  VM *vm = (VM*)env->aux;
  if (args[0].kind!=VAL_STR) return v_unit();
  size_t len=0; char *buf = read_file_all(args[0].as.str->data, &len);
  if (!buf) return v_unit();
  String *s = rt_string_new(vm, buf, len); free(buf);
  return v_str(s);
}

Value rt_write_file(Env *env, Value *args, int nargs) {
  (void)env; if (!expect_nargs(nargs,2,"write-file")) return v_unit();
  if (args[0].kind!=VAL_STR || args[1].kind!=VAL_STR) return v_unit();
  FILE *f = fopen(args[0].as.str->data, "wb"); if (!f) return v_bool(false);
  fwrite(args[1].as.str->data,1,(size_t)args[1].as.str->len,f); fclose(f);
  return v_bool(true);
}

Value rt_str_split_ws(Env *env, Value *args, int nargs) {
  VM *vm = (VM*)env->aux; if (nargs!=1 || args[0].kind!=VAL_STR) return v_vec(NULL);
  const char *s = args[0].as.str->data; size_t n = (size_t)args[0].as.str->len;
  Vector *v = (Vector*)gc_alloc(&vm->gc, sizeof(Vector), 4); v->len=0; v->cap=8; v->items=(Value*)malloc(sizeof(Value)*v->cap);
  size_t i=0; while (i<n) {
    while (i<n && (s[i]==' '||s[i]=='\n' || s[i]=='\t' || s[i]=='\r')) i++;
    size_t start=i; while (i<n && !(s[i]==' '||s[i]=='\n'||s[i]=='\t'||s[i]=='\r')) i++;
    if (i>start) { String *str = rt_string_new(vm, s+start, i-start); if (v->len==v->cap){v->cap*=2; v->items=(Value*)realloc(v->items,sizeof(Value)*v->cap);} v->items[v->len++] = v_str(str); }
  }
  return v_vec(v);
}

static int both_int(Value a, Value b) { return a.kind==VAL_INT && b.kind==VAL_INT; }
static int both_float(Value a, Value b) { return a.kind==VAL_FLOAT && b.kind==VAL_FLOAT; }

Value rt_add(Env *env, Value *args, int nargs) {
  (void)env; if (nargs!=2) return v_unit();
  if (both_int(args[0],args[1])) return v_int(args[0].as.i + args[1].as.i);
  if (both_float(args[0],args[1])) return v_float(args[0].as.f + args[1].as.f);
  return v_unit();
}
Value rt_sub(Env *env, Value *args, int nargs) {
  (void)env; if (nargs!=2) return v_unit();
  if (both_int(args[0],args[1])) return v_int(args[0].as.i - args[1].as.i);
  if (both_float(args[0],args[1])) return v_float(args[0].as.f - args[1].as.f);
  return v_unit();
}
Value rt_mul(Env *env, Value *args, int nargs) {
  (void)env; if (nargs!=2) return v_unit();
  if (both_int(args[0],args[1])) return v_int(args[0].as.i * args[1].as.i);
  if (both_float(args[0],args[1])) return v_float(args[0].as.f * args[1].as.f);
  return v_unit();
}
Value rt_div(Env *env, Value *args, int nargs) {
  (void)env; if (nargs!=2) return v_unit();
  if (both_int(args[0],args[1])) return v_int(args[0].as.i / args[1].as.i);
  if (both_float(args[0],args[1])) return v_float(args[0].as.f / args[1].as.f);
  return v_unit();
}

Value rt_eq(Env *env, Value *args, int nargs) {
  (void)env; if (nargs!=2) return v_bool(false);
  Value a=args[0], b=args[1];
  if (a.kind!=b.kind) return v_bool(false);
  switch (a.kind) {
    case VAL_INT: return v_bool(a.as.i==b.as.i);
    case VAL_FLOAT: return v_bool(a.as.f==b.as.f);
    case VAL_BOOL: return v_bool(a.as.b==b.as.b);
    case VAL_STR: return v_bool(a.as.str->len==b.as.str->len && memcmp(a.as.str->data,b.as.str->data,a.as.str->len)==0);
    case VAL_UNIT: return v_bool(true);
    default: return v_bool(false);
  }
}
Value rt_lt(Env *env, Value *args, int nargs) {
  (void)env; if (nargs!=2) return v_bool(false);
  Value a=args[0], b=args[1];
  if (a.kind==VAL_INT && b.kind==VAL_INT) return v_bool(a.as.i<b.as.i);
  if (a.kind==VAL_FLOAT && b.kind==VAL_FLOAT) return v_bool(a.as.f<b.as.f);
  return v_bool(false);
}
Value rt_gt(Env *env, Value *args, int nargs) {
  (void)env; if (nargs!=2) return v_bool(false);
  Value a=args[0], b=args[1];
  if (a.kind==VAL_INT && b.kind==VAL_INT) return v_bool(a.as.i>b.as.i);
  if (a.kind==VAL_FLOAT && b.kind==VAL_FLOAT) return v_bool(a.as.f>b.as.f);
  return v_bool(false);
}

Value rt_chan(Env *env, Value *args, int nargs) {
  (void)env; (void)args; if (nargs!=0) { /* type info not used at runtime here */ }
  Channel *c = rt_channel_new(16);
  return v_chan(c);
}

typedef struct { VM *vm; Closure *clos; } SpawnArg;
static void *spawn_tramp(void *p) {
  SpawnArg *sa=(SpawnArg*)p; vm_call_closure_noargs(sa->vm, sa->clos); free(sa); return NULL;
}

Value rt_spawn(Env *env, Value *args, int nargs) {
  if (nargs!=1 || args[0].kind!=VAL_CLOSURE) return v_unit();
  VM *vm = (VM*)env->aux;
  SpawnArg *sa = (SpawnArg*)malloc(sizeof(SpawnArg)); sa->vm=vm; sa->clos=args[0].as.clos;
  RtThread *t = rt_thread_spawn(spawn_tramp, sa);
  (void)t; // fire-and-forget; later we can add join/handle
  return v_unit();
}

Value rt_send(Env *env, Value *args, int nargs) {
  (void)env; if (nargs!=2 || args[0].kind!=VAL_CHAN) return v_bool(false);
  Value *box = (Value*)malloc(sizeof(Value)); *box = args[1];
  bool ok = rt_channel_send(args[0].as.chan, box, -1);
  return v_bool(ok);
}
Value rt_recv(Env *env, Value *args, int nargs) {
  (void)env; if (nargs!=1 || args[0].kind!=VAL_CHAN) return v_unit();
  Value *box = (Value*)rt_channel_recv(args[0].as.chan, -1);
  if (!box) return v_unit();
  Value v=*box; free(box); return v;
}

// Collections
static Vector *vec_new_gc(VM *vm, int cap) {
  Vector *v = (Vector*)gc_alloc(&vm->gc, sizeof(Vector), 4); v->len=0; v->cap=cap>0?cap:4; v->items=(Value*)malloc(sizeof(Value)*v->cap); return v; }
Value rt_vec_new(Env *env, Value *args, int nargs) {
  VM *vm=(VM*)env->aux; Vector *v = vec_new_gc(vm, nargs);
  for (int i=0;i<nargs;i++) v->items[v->len++]=args[i];
  return v_vec(v);
}
Value rt_vec_push(Env *env, Value *args, int nargs) {
  (void)env; (void)nargs; if (nargs!=2 || args[0].kind!=VAL_VEC) return v_unit();
  Vector *v=args[0].as.vec; if (v->len==v->cap){ v->cap*=2; v->items=(Value*)realloc(v->items,sizeof(Value)*v->cap);} v->items[v->len++]=args[1]; return v_unit();
}
Value rt_vec_get(Env *env, Value *args, int nargs) {
  (void)env; if (nargs!=2 || args[0].kind!=VAL_VEC || args[1].kind!=VAL_INT) return v_unit();
  Vector *v=args[0].as.vec; int64_t i=args[1].as.i; if (i<0 || i>=v->len) return v_unit(); return v->items[i];
}
Value rt_vec_len(Env *env, Value *args, int nargs) { (void)env; if (nargs!=1 || args[0].kind!=VAL_VEC) return v_int(0); return v_int(args[0].as.vec->len); }

// Code-as-data list/symbol helpers
Value rt_is_list(Env *env, Value *args, int nargs){ (void)env; if (nargs!=1) return v_bool(false); return v_bool(args[0].kind==VAL_LIST); }
Value rt_is_symbol(Env *env, Value *args, int nargs){ (void)env; if (nargs!=1) return v_bool(false); return v_bool(args[0].kind==VAL_SYMBOL); }
Value rt_symbol_eq(Env *env, Value *args, int nargs){ (void)env; if (nargs!=2) return v_bool(false); if (args[0].kind!=VAL_SYMBOL || args[1].kind!=VAL_SYMBOL) return v_bool(false); if (args[0].as.sym.len!=args[1].as.sym.len) return v_bool(false); return v_bool(strncmp(args[0].as.sym.name,args[1].as.sym.name,args[0].as.sym.len)==0); }
Value rt_list_len(Env *env, Value *args, int nargs){ (void)env; if (nargs!=1 || args[0].kind!=VAL_LIST || !args[0].as.list) return v_int(0); return v_int(args[0].as.list->len); }
Value rt_list_head(Env *env, Value *args, int nargs){ (void)env; if (nargs!=1 || args[0].kind!=VAL_LIST || !args[0].as.list || args[0].as.list->len==0) return v_unit(); return args[0].as.list->items[0]; }
Value rt_list_tail(Env *env, Value *args, int nargs){ (void)env; if (nargs!=1 || args[0].kind!=VAL_LIST || !args[0].as.list) return v_list(NULL); VM *vm=(VM*)env->aux; ValList *l=args[0].as.list; if (l->len<=1){ ValList *nl=(ValList*)gc_alloc(&vm->gc,sizeof(ValList),8); nl->len=0; nl->cap=0; nl->items=NULL; return v_list(nl);} ValList *nl=(ValList*)gc_alloc(&vm->gc,sizeof(ValList),8); nl->len=l->len-1; nl->cap=nl->len; nl->items=(Value*)malloc(sizeof(Value)*nl->len); for (int i=0;i<nl->len;i++) nl->items[i]=l->items[i+1]; return v_list(nl); }
Value rt_list_cons(Env *env, Value *args, int nargs){ if (nargs!=2 || args[1].kind!=VAL_LIST) return v_list(NULL); VM *vm=(VM*)env->aux; ValList *l=args[1].as.list; int n = l?l->len:0; ValList *nl=(ValList*)gc_alloc(&vm->gc,sizeof(ValList),8); nl->len=n+1; nl->cap=nl->len; nl->items=(Value*)malloc(sizeof(Value)*nl->len); nl->items[0]=args[0]; for (int i=0;i<n;i++) nl->items[i+1]=l->items[i]; return v_list(nl);} 
Value rt_list_append(Env *env, Value *args, int nargs){ if (nargs!=2 || args[0].kind!=VAL_LIST || args[1].kind!=VAL_LIST) return v_list(NULL); VM *vm=(VM*)env->aux; ValList *a=args[0].as.list; ValList *b=args[1].as.list; int na=a?a->len:0, nb=b?b->len:0; ValList *nl=(ValList*)gc_alloc(&vm->gc,sizeof(ValList),8); nl->len=na+nb; nl->cap=nl->len; nl->items=(Value*)malloc(sizeof(Value)*nl->len); for (int i=0;i<na;i++) nl->items[i]=a->items[i]; for (int j=0;j<nb;j++) nl->items[na+j]=b->items[j]; return v_list(nl);} 
// very simple map (Str->Int) with linear probing
static uint64_t hash_str(const char *s, int64_t len){ uint64_t h=1469598103934665603ull; for (int64_t i=0;i<len;i++){ h^=(unsigned char)s[i]; h*=1099511628211ull; } return h; }
static Map *map_new_gc(VM *vm, int cap){ Map *m=(Map*)gc_alloc(&vm->gc,sizeof(Map),5); m->cap=cap>8?cap:8; m->len=0; m->slots=(MapEntry*)calloc(m->cap,sizeof(MapEntry)); return m; }
static void map_set_pair(Map *m, String *k, int64_t val){
  uint64_t h = hash_str(k->data,k->len); int i = (int)(h % m->cap);
  while (m->slots[i].used) {
    Value *kv = m->slots[i].key; if (kv && kv->kind==VAL_STR) {
      if (kv->as.str->len==k->len && memcmp(kv->as.str->data,k->data,k->len)==0) { m->slots[i].val->kind=VAL_INT; m->slots[i].val->as.i=val; return; }
    }
    i=(i+1)%m->cap;
  }
  m->slots[i].used=1; m->slots[i].key=(Value*)malloc(sizeof(Value)); *m->slots[i].key = v_str(k); m->slots[i].val=(Value*)malloc(sizeof(Value)); *m->slots[i].val = v_int(val); m->len++;
}
static int map_get_pair(Map *m, String *k, int64_t *out){ uint64_t h=hash_str(k->data,k->len); int i=(int)(h%m->cap); int start=i; while (m->slots[i].used){ Value *kv=m->slots[i].key; if (kv && kv->kind==VAL_STR && kv->as.str->len==k->len && memcmp(kv->as.str->data,k->data,k->len)==0){ *out = m->slots[i].val->as.i; return 1; } i=(i+1)%m->cap; if (i==start) break; } return 0; }

Value rt_map_new(Env *env, Value *args, int nargs){ (void)args; (void)nargs; VM *vm=(VM*)env->aux; Map *m=map_new_gc(vm, 16); return v_map(m);} 
Value rt_map_set(Env *env, Value *args, int nargs){ (void)env; if (nargs!=3 || args[0].kind!=VAL_MAP || args[1].kind!=VAL_STR || args[2].kind!=VAL_INT) return v_unit(); map_set_pair(args[0].as.map, args[1].as.str, args[2].as.i); return v_unit(); }
Value rt_map_get(Env *env, Value *args, int nargs){ (void)env; if (nargs!=2 || args[0].kind!=VAL_MAP || args[1].kind!=VAL_STR) return v_int(0); int64_t out=0; if (map_get_pair(args[0].as.map,args[1].as.str,&out)) return v_int(out); return v_int(0);} 
Value rt_map_len(Env *env, Value *args, int nargs){ (void)env; if (nargs!=1 || args[0].kind!=VAL_MAP) return v_int(0); return v_int(args[0].as.map->len);} 
