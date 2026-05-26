#ifndef ALIAS_H
#define ALIAS_H

void set_alias(const char *name, const char *value);
void unset_alias(const char *name);
char *get_alias(const char *name);
void print_aliases(void);

#endif
