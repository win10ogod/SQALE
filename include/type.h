#ifndef TYPE_H
#define TYPE_H

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>

typedef enum {
  TY_INT,
  TY_FLOAT,
  TY_BOOL,
  TY_STR,
  TY_UNIT,
  TY_ANY,
  TY_FUNC,
  TY_CHAN, // Channel of element type
  TY_VEC,
  TY_MAP,
  TY_OPTION, // Option[T] - Some(T) or None
  TY_RESULT, // Result[T,E] - Ok(T) or Err(E)
  TY_STRUCT, // Named struct type
  TY_ENUM,   // Enum type
  TY_ERROR,
} TypeKind;

typedef struct Type Type;

struct Type {
  TypeKind kind;
  union {
    struct { Type **params; size_t arity; Type *ret; } fn;
    struct { Type *elem; } chan;
    struct { Type *ok_type; Type *err_type; } result; // For TY_RESULT
    struct { const char *name; Type **fields; const char **field_names; size_t nfields; } struc; // For TY_STRUCT
    struct { const char *name; const char **variants; size_t nvariants; } enu; // For TY_ENUM
  } as;
};

typedef struct TypeArena {
  struct Type *all; // not used; simplified per-arena allocated via arena.h
} TypeArena;

// Constructors (allocated from arena)
Type *ty_int(void *arena);
Type *ty_float(void *arena);
Type *ty_bool(void *arena);
Type *ty_str(void *arena);
Type *ty_unit(void *arena);
Type *ty_any(void *arena);
Type *ty_error(void *arena);
Type *ty_func(void *arena, Type **params, size_t arity, Type *ret);
Type *ty_chan(void *arena, Type *elem);
Type *ty_vec(void *arena, Type *elem);
Type *ty_map(void *arena, Type *key, Type *val);
Type *ty_option(void *arena, Type *elem);
Type *ty_result(void *arena, Type *ok_type, Type *err_type);
Type *ty_struct(void *arena, const char *name, Type **fields, const char **field_names, size_t nfields);
Type *ty_enum(void *arena, const char *name, const char **variants, size_t nvariants);

// Utilities
bool ty_eq(const Type *a, const Type *b);
const char *ty_kind_name(TypeKind k);
void ty_to_string(const Type *t, char *buf, size_t bufsize);

#endif // TYPE_H
