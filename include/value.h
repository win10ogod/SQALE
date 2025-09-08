#ifndef VALUE_H
#define VALUE_H

#include <stdint.h>
#include <stdbool.h>
#include "type.h"
#include "gc.h"

typedef struct Obj Obj;

typedef enum {
  VAL_INT,
  VAL_FLOAT,
  VAL_BOOL,
  VAL_STR,
  VAL_FUNC,    // builtin/native
  VAL_CLOSURE, // user fn with env capture
  VAL_UNIT,
  VAL_CHAN,
  VAL_SYMBOL,
  VAL_LIST,
  VAL_VEC,
  VAL_MAP,
} ValueKind;

typedef struct Value Value;

typedef struct Env Env;

typedef Value (*NativeFn)(Env *env, Value *args, int nargs);

typedef struct String {
  Obj hdr;
  char *data;
  int64_t len;
} String;

typedef struct Channel Channel;

typedef struct Closure {
  Obj hdr;
  // function AST pointer and environment
  void *fn_node; // opaque pointer to AST node for [fn ...]
  Env *env;      // captured environment
  Type *type;    // function type
} Closure;

typedef struct ValList {
  Obj hdr;
  struct Value *items;
  int32_t len;
  int32_t cap;
} ValList;

typedef struct Vector {
  Obj hdr;
  struct Value *items;
  int32_t len;
  int32_t cap;
} Vector;

struct Value; // forward for MapEntry
typedef struct MapEntry { struct Value *key; struct Value *val; int used; } MapEntry;
typedef struct Map {
  Obj hdr;
  MapEntry *slots;
  int32_t cap;
  int32_t len;
} Map;

struct Value {
  ValueKind kind;
  union {
    int64_t i;
    double f;
    bool b;
    String *str;
    struct { NativeFn fn; Type *type; } native;
    Closure *clos;
    Channel *chan;
    struct { const char *name; int32_t len; } sym;
    ValList *list;
    Vector *vec;
    Map *map;
  } as;
};

// Constructors
Value v_int(int64_t x);
Value v_float(double x);
Value v_bool(bool x);
Value v_unit(void);
Value v_str(String *s);
Value v_native(NativeFn f, Type *type);
Value v_closure(Closure *c);
Value v_chan(Channel *c);
Value v_symbol(const char *name, int32_t len);
Value v_list(ValList *l);
Value v_vec(Vector *v);
Value v_map(Map *m);

#endif // VALUE_H
