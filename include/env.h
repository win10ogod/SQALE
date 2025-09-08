#ifndef ENV_H
#define ENV_H

#include <stddef.h>
#include <stdbool.h>
#include "type.h"

typedef struct EnvEntry {
  const char *name;
  Type *type;
  void *value; // runtime value (optional for type env)
  struct EnvEntry *next;
} EnvEntry;

typedef struct Env {
  EnvEntry *head;
  struct Env *parent;
  void *aux; // VM* or other context propagated to children
} Env;

Env *env_new(Env *parent);
void env_free(Env *e); // frees only entries, not names or values
bool env_set(Env *e, const char *name, Type *type, void *value);
EnvEntry *env_lookup(Env *e, const char *name);

#endif // ENV_H
