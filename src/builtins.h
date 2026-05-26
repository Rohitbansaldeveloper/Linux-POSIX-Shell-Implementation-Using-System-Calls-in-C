#ifndef BUILTINS_H
#define BUILTINS_H

#include "parser.h"

int is_builtin(const char *cmd);
int execute_builtin(Command *cmd);

#endif
