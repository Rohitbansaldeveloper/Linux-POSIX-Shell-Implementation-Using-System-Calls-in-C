#include "alias.h"
#include "string_utils.h"
#include "memory.h"

#define MAX_ALIASES 64
static char *aliases[MAX_ALIASES];
static int alias_count = 0;

void set_alias(const char *name, const char *value) {
    size_t name_len = str_len(name);
    size_t val_len = str_len(value);
    size_t total = name_len + val_len + 2;
    
    char *entry = mem_alloc(total);
    str_cpy(entry, name);
    entry[name_len] = '=';
    str_cpy(entry + name_len + 1, value);
    
    for (int i = 0; i < alias_count; i++) {
        if (str_ncmp(aliases[i], name, name_len) == 0 && aliases[i][name_len] == '=') {
            aliases[i] = entry;
            return;
        }
    }
    if (alias_count < MAX_ALIASES) {
        aliases[alias_count++] = entry;
    }
}

void unset_alias(const char *name) {
    size_t len = str_len(name);
    for (int i = 0; i < alias_count; i++) {
        if (str_ncmp(aliases[i], name, len) == 0 && aliases[i][len] == '=') {
            for (int j = i; j < alias_count - 1; j++) {
                aliases[j] = aliases[j+1];
            }
            alias_count--;
            return;
        }
    }
}

char *get_alias(const char *name) {
    size_t len = str_len(name);
    for (int i = 0; i < alias_count; i++) {
        if (str_ncmp(aliases[i], name, len) == 0 && aliases[i][len] == '=') {
            return &aliases[i][len + 1];
        }
    }
    return NULL;
}

void print_aliases(void) {
    for (int i = 0; i < alias_count; i++) {
        print_str(1, "alias ");
        print_str(1, aliases[i]);
        print_str(1, "\n");
    }
}
