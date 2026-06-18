#ifndef ASTER_ENV_H
#define ASTER_ENV_H

#include <stdbool.h>

#include "value.h"

/* Lexical environment for variable bindings with parent chaining */
typedef struct Env {
    char* keys[256];
    Value values[256];
    int count;
    struct Env* parent;
} Env;

/* Creates a new environment optionally linked to a parent scope */
Env* envCreate(Env* parent);

/* Frees an environment and all bindings it owns */
void envFree(Env* env);

/* Defines a new variable in the current scope, failing if the name exists */
bool envDefine(Env* env, const char* key, Value value);

/* Assigns to an existing binding in the current or ancestor scope */
bool envSet(Env* env, const char* key, Value value);

/* Looks up a variable by name, copying the value into out on success */
bool envGet(Env* env, const char* key, Value* out);

#endif /* ASTER_ENV_H */
