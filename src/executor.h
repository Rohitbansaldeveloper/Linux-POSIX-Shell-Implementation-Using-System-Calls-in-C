#ifndef EXECUTOR_H
#define EXECUTOR_H

#include "parser.h"

int execute_pipeline(Pipeline *pipeline);
int execute_ast(ASTNode *node);
void execute_string(const char *str, int out_fd);

#endif
