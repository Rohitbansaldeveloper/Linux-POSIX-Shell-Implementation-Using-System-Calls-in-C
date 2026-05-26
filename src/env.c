/*
 * env.c
 * 
 * Manages the shell's environment variables dynamically.
 */

#include "env.h"
#include "memory.h"
#include "string_utils.h"

static char *dynamic_env[MAX_ENV_VARS];
static int env_count = 0;

void init_env(char **envp) {
    env_count = 0;
    while (envp[env_count] != NULL && env_count < MAX_ENV_VARS - 1) {
        size_t len = str_len(envp[env_count]);
        dynamic_env[env_count] = mem_alloc(len + 1);
        str_cpy(dynamic_env[env_count], envp[env_count]);
        env_count++;
    }
    dynamic_env[env_count] = NULL;
}

char **get_env_array(void) {
    return dynamic_env;
}

char *get_env_val(const char *name) {
    size_t len = str_len(name);
    for (int i = 0; i < env_count; i++) {
        if (str_ncmp(dynamic_env[i], name, len) == 0 && dynamic_env[i][len] == '=') {
            return &dynamic_env[i][len + 1];
        }
    }
    return NULL;
}

int set_env_val(const char *name, const char *value) {
    size_t name_len = str_len(name);
    size_t val_len = str_len(value);
    size_t total_len = name_len + val_len + 2; // name + '=' + value + '\0'
    
    char *new_entry = mem_alloc(total_len);
    if (!new_entry) return -1;
    
    str_cpy(new_entry, name);
    new_entry[name_len] = '=';
    str_cpy(new_entry + name_len + 1, value);
    
    // Check if it exists
    for (int i = 0; i < env_count; i++) {
        if (str_ncmp(dynamic_env[i], name, name_len) == 0 && dynamic_env[i][name_len] == '=') {
            dynamic_env[i] = new_entry;
            return 0;
        }
    }
    
    // Append new
    if (env_count < MAX_ENV_VARS - 1) {
        dynamic_env[env_count++] = new_entry;
        dynamic_env[env_count] = NULL;
        return 0;
    }
    
    return -1; // Full
}
