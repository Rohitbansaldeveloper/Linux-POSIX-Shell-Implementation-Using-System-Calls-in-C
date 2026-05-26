#ifndef STRING_UTILS_H
#define STRING_UTILS_H

#include "syscalls.h"

#define NULL ((void *)0)

size_t str_len(const char *str);
int str_cmp(const char *s1, const char *s2);
int str_ncmp(const char *s1, const char *s2, size_t n);
char *str_cpy(char *dest, const char *src);
char *str_chr(const char *str, int c);
char *str_str(const char *haystack, const char *needle);
int glob_match(const char *pattern, const char *str);
void *mem_cpy(void *dest, const void *src, size_t n);
void *mem_set(void *s, int c, size_t n);
void print_str(int fd, const char *str);
void print_int(int fd, int n);

#endif
