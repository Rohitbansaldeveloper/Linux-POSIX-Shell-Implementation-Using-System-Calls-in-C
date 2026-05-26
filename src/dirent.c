/*
 * dirent.c
 * 
 * Provides raw directory reading capabilities using the getdents64 system call.
 * We use this to implement "TAB completion" without relying on libc's opendir/readdir.
 */

#include "dirent.h"
#include "syscalls.h"
#include "string_utils.h"

// O_DIRECTORY ensures the open call fails if the target is not a directory.
#define O_RDONLY    00
#define O_DIRECTORY 0200000

#define BUF_SIZE 4096

int autocomplete_match(const char *prefix, char *out_buffer, int max_len) {
    // Open the current working directory
    int fd = sys_open(".", O_RDONLY | O_DIRECTORY, 0);
    if (fd < 0) return 0;
    
    char buf[BUF_SIZE];
    size_t prefix_len = str_len(prefix);
    int found = 0;
    
    while (1) {
        // Read raw directory entries directly from the kernel
        int nread = sys_getdents64(fd, buf, BUF_SIZE);
        if (nread <= 0) {
            break; // End of directory stream or error
        }
        
        int bpos = 0;
        while (bpos < nread) {
            // Cast the raw bytes at the current offset to the dirent structure
            struct linux_dirent64 *d = (struct linux_dirent64 *) (buf + bpos);
            
            // Skip the current and parent directory pointers
            if (str_cmp(d->d_name, ".") != 0 && str_cmp(d->d_name, "..") != 0) {
                // Check if the filename starts with the user's typed prefix
                if (str_ncmp(d->d_name, prefix, prefix_len) == 0) {
                    
                    // For a minimal shell, we just return the very first match found.
                    int name_len = str_len(d->d_name);
                    if (name_len < max_len) {
                        str_cpy(out_buffer, d->d_name);
                        found = 1;
                        break;
                    }
                }
            }
            // Advance the byte position by the record length specified by the kernel
            bpos += d->d_reclen;
        }
        if (found) break; // Break out of the outer read loop if we got a match
    }
    
    sys_close(fd);
    return found;
}
