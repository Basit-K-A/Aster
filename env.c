#include "env.h"

#include <stdlib.h>
#include <string.h>

/* Creates a new environment optionally linked to a parent scope */
Env* envCreate(Env* parent) {
    Env* env = (Env*)calloc(1, sizeof(Env));
    if (!env) return NULL;
    env->parent = parent;
    return env;
}

/* Frees an environment and all bindings it owns */
void envFree(Env* env) {
    if (!env) return;
    for (int i = 0; i < env->count; i++) {
        free(env->keys[i]);
        if (env->values[i].type == VAL_FUNCTION) {
            functionFree(env->values[i].as.function);
        } else {
            valueFree(env->values[i]);
        }
    }
    free(env);
}

/* Defines a new variable in the current scope, failing if the name exists */
bool envDefine(Env* env, const char* key, Value value) {
    if (!env || env->count >= 256) return false;

    for (int i = 0; i < env->count; i++) {
        if (strcmp(env->keys[i], key) == 0) return false;
    }

    env->keys[env->count] = strdup(key);
    env->values[env->count] = valueCopy(value);
    env->count++;
    return true;
}

/* Assigns to an existing binding in the current or ancestor scope */
bool envSet(Env* env, const char* key, Value value) {
    if (!env) return false;

    for (int i = 0; i < env->count; i++) {
        if (strcmp(env->keys[i], key) == 0) {
            valueFree(env->values[i]);
            env->values[i] = valueCopy(value);
            return true;
        }
    }

    if (env->parent) return envSet(env->parent, key, value);
    return false;
}

/* Looks up a variable by name, copying the value into out on success */
bool envGet(Env* env, const char* key, Value* out) {
    if (!env) return false;

    for (int i = 0; i < env->count; i++) {
        if (strcmp(env->keys[i], key) == 0) {
            *out = valueCopy(env->values[i]);
            return true;
        }
    }

    if (env->parent) return envGet(env->parent, key, out);
    return false;
}
