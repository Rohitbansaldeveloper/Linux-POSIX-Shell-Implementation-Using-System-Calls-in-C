#ifndef MEMORY_H
#define MEMORY_H

#include "syscalls.h"

void *mem_alloc(size_t size);
void *mem_alloc_temp(size_t size);
void mem_free(void *ptr);
void mem_init(void);
void mem_reset_temp(void);

#endif
