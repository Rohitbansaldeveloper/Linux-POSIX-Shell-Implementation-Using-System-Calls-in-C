#include "string_utils.h"

size_t str_len(const char *str) {
    size_t len = 0;
    while (str[len]) {
        len++;
    }
    return len;
}

char *str_chr(const char *str, int c) {
    while (*str) {
        if (*str == c) return (char *)str;
        str++;
    }
    return NULL;
}

char *str_str(const char *haystack, const char *needle) {
    if (!*needle) return (char *)haystack;
    while (*haystack) {
        const char *h = haystack;
        const char *n = needle;
        while (*h && *n && *h == *n) {
            h++;
            n++;
        }
        if (!*n) return (char *)haystack;
        haystack++;
    }
    return NULL;
}

int glob_match(const char *pattern, const char *str) {
    if (*pattern == '\0') return *str == '\0';
    if (*pattern == '*') {
        if (glob_match(pattern + 1, str)) return 1;
        if (*str != '\0' && glob_match(pattern, str + 1)) return 1;
        return 0;
    }
    if (*pattern == '?' || *pattern == *str) {
        if (*str == '\0') return 0;
        return glob_match(pattern + 1, str + 1);
    }
    return 0;
}

int str_cmp(const char *s1, const char *s2) {
    while (*s1 && (*s1 == *s2)) {
        s1++;
        s2++;
    }
    return *(const unsigned char *)s1 - *(const unsigned char *)s2;
}

int str_ncmp(const char *s1, const char *s2, size_t n) {
    if (n == 0) return 0;
    while (n-- > 0 && *s1 && (*s1 == *s2)) {
        if (n == 0) break;
        s1++;
        s2++;
    }
    return *(const unsigned char *)s1 - *(const unsigned char *)s2;
}

char *str_cpy(char *dest, const char *src) {
    char *d = dest;
    while ((*d++ = *src++))
        ;
    return dest;
}

void *mem_cpy(void *dest, const void *src, size_t n) {
    char *d = (char *)dest;
    const char *s = (const char *)src;
    while (n--) {
        *d++ = *s++;
    }
    return dest;
}

void *mem_set(void *s, int c, size_t n) {
    unsigned char *p = (unsigned char *)s;
    while (n--) {
        *p++ = (unsigned char)c;
    }
    return s;
}

void print_str(int fd, const char *str) {
    sys_write(fd, str, str_len(str));
}

void print_int(int fd, int n) {
    char buf[32];
    int i = 30;
    int is_neg = n < 0;
    
    if (n == 0) {
        sys_write(fd, "0", 1);
        return;
    }
    
    unsigned int un = is_neg ? -n : n;
    
    buf[31] = '\0';
    while (un > 0 && i >= 0) {
        buf[i--] = '0' + (un % 10);
        un /= 10;
    }
    if (is_neg && i >= 0) {
        buf[i--] = '-';
    }
    sys_write(fd, &buf[i + 1], 30 - i);
}
