#include "env.h"
#include <stdlib.h>
#include <string.h>

Env *env_new(Env *parent) {
  Env *e = (Env*)malloc(sizeof(Env));
  e->head = NULL; e->parent = parent; e->aux = parent ? parent->aux : NULL; return e;
}

void env_free(Env *e) {
  EnvEntry *cur = e->head;
  while (cur) { EnvEntry *n = cur->next; free(cur); cur = n; }
  free(e);
}

bool env_set(Env *e, const char *name, Type *type, void *value) {
  EnvEntry *en = (EnvEntry*)malloc(sizeof(EnvEntry));
  en->name = name; en->type = type; en->value = value; en->next = e->head; e->head = en; return true;
}

EnvEntry *env_lookup(Env *e, const char *name) {
  for (Env *cur=e; cur; cur=cur->parent) {
    for (EnvEntry *en=cur->head; en; en=en->next) {
      if (strcmp(en->name, name)==0) return en;
    }
  }
  return NULL;
}
