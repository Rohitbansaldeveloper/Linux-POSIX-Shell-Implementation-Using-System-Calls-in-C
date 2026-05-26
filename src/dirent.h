#ifndef DIRENT_H
#define DIRENT_H

#include "syscalls.h"

// The structure representing a directory entry returned by getdents64 on x86_64
struct linux_dirent64 {
    unsigned long  d_ino;    // 64-bit inode number
    long           d_off;    // 64-bit offset to next structure
    unsigned short d_reclen; // Size of this dirent
    unsigned char  d_type;   // File type
    char           d_name[]; // Filename (null-terminated)
};

// Attempts to find a filename in the current directory that starts with 'prefix'.
// Copies the matching filename into 'out_buffer' if found.
// Returns 1 if a match is found, 0 otherwise.
int autocomplete_match(const char *prefix, char *out_buffer, int max_len);

#endif
