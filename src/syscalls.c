/*
 * syscalls.c
 * 
 * This file implements the raw interface to the Linux kernel via system calls.
 * Because we are not using the C standard library (libc), we cannot call standard
 * functions like read(), fork(), or execve(). Instead, we must trigger a software
 * interrupt to ask the kernel to perform these actions for us.
 * 
 * On x86_64 Linux, system calls are invoked using the 'syscall' instruction.
 * The kernel expects arguments in specific CPU registers:
 * - %rax: System call number (e.g., 0 for read, 59 for execve)
 * - %rdi: First argument
 * - %rsi: Second argument
 * - %rdx: Third argument
 * - %r10: Fourth argument (Note: libc wrappers often use rcx, but the kernel needs r10)
 * - %r8:  Fifth argument
 * - %r9:  Sixth argument
 * 
 * The return value from the system call is placed in %rax.
 */

#include "syscalls.h"

// 0 arguments syscall
long syscall0(long n) {
    long ret;
    __asm__ volatile(
        "syscall"
        : "=a"(ret)             // Output: ret gets the value from %rax
        : "a"(n)                // Input: %rax gets the syscall number (n)
        : "rcx", "r11", "memory"// Clobbered registers (destroyed by the syscall instruction)
    );
    return ret;
}

// 1 argument syscall
long syscall1(long n, long a1) {
    long ret;
    __asm__ volatile(
        "syscall"
        : "=a"(ret)
        : "a"(n), "D"(a1)       // "D" constraint puts a1 into %rdi
        : "rcx", "r11", "memory"
    );
    return ret;
}

// 2 arguments syscall
long syscall2(long n, long a1, long a2) {
    long ret;
    __asm__ volatile(
        "syscall"
        : "=a"(ret)
        : "a"(n), "D"(a1), "S"(a2) // "S" constraint puts a2 into %rsi
        : "rcx", "r11", "memory"
    );
    return ret;
}

// 3 arguments syscall
long syscall3(long n, long a1, long a2, long a3) {
    long ret;
    __asm__ volatile(
        "syscall"
        : "=a"(ret)
        : "a"(n), "D"(a1), "S"(a2), "d"(a3) // "d" constraint puts a3 into %rdx
        : "rcx", "r11", "memory"
    );
    return ret;
}

// 4 arguments syscall
long syscall4(long n, long a1, long a2, long a3, long a4) {
    long ret;
    // The C compiler doesn't have a simple letter constraint for r10, 
    // so we explicitly map a variable to the r10 register.
    register long r10 __asm__("r10") = a4;
    __asm__ volatile(
        "syscall"
        : "=a"(ret)
        : "a"(n), "D"(a1), "S"(a2), "d"(a3), "r"(r10)
        : "rcx", "r11", "memory"
    );
    return ret;
}

// 6 arguments syscall (maximum allowed for Linux)
long syscall6(long n, long a1, long a2, long a3, long a4, long a5, long a6) {
    long ret;
    register long r10 __asm__("r10") = a4;
    register long r8  __asm__("r8")  = a5;
    register long r9  __asm__("r9")  = a6;
    __asm__ volatile(
        "syscall"
        : "=a"(ret)
        : "a"(n), "D"(a1), "S"(a2), "d"(a3), "r"(r10), "r"(r8), "r"(r9)
        : "rcx", "r11", "memory"
    );
    return ret;
}

/* 
 * Specific syscall wrappers used by our shell 
 * These wrap the generic assembly functions with the exact syscall numbers 
 * defined in the Linux kernel headers.
 */

// sys_read: Reads 'count' bytes from file descriptor 'fd' into 'buf'.
ssize_t sys_read(int fd, void *buf, size_t count) {
    return syscall3(SYS_read, fd, (long)buf, count);
}

// sys_write: Writes 'count' bytes from 'buf' to file descriptor 'fd'.
ssize_t sys_write(int fd, const void *buf, size_t count) {
    return syscall3(SYS_write, fd, (long)buf, count);
}

// sys_open: Opens a file and returns a new file descriptor.
int sys_open(const char *filename, int flags, int mode) {
    return syscall3(SYS_open, (long)filename, flags, mode);
}

// sys_kill: Sends a signal to a process.
int sys_kill(pid_t pid, int sig) {
    return syscall2(SYS_kill, pid, sig);
}

// sys_getdents64: Reads raw directory entries from a file descriptor into a buffer.
int sys_getdents64(unsigned int fd, void *dirp, unsigned int count) {
    return syscall3(SYS_getdents64, fd, (long)dirp, count);
}

// sys_setpgid: Set process group ID for job control.
int sys_setpgid(pid_t pid, pid_t pgid) {
    return syscall2(SYS_setpgid, pid, pgid);
}

// sys_getpgrp: Get current process group ID.
pid_t sys_getpgrp(void) {
    return syscall0(SYS_getpgrp);
}

pid_t sys_getpid(void) {
    return syscall0(SYS_getpid);
}

// sys_close: Closes an open file descriptor.
int sys_close(int fd) {
    return syscall1(SYS_close, fd);
}

// sys_mmap: Maps files or devices into memory, or allocates anonymous memory.
void *sys_mmap(void *addr, size_t length, int prot, int flags, int fd, long offset) {
    return (void *)syscall6(SYS_mmap, (long)addr, length, prot, flags, fd, offset);
}

// sys_munmap: Unmaps a memory region.
int sys_munmap(void *addr, size_t length) {
    return syscall2(SYS_munmap, (long)addr, length);
}

// sys_ioctl: Device control interface (used for terminal raw mode)
int sys_ioctl(int fd, unsigned long request, void *argp) {
    return syscall3(SYS_ioctl, fd, request, (long)argp);
}

// sys_pipe: Creates a unidirectional data channel (pipe). 
// pipefd[0] is for reading, pipefd[1] is for writing.
int sys_pipe(int pipefd[2]) {
    return syscall1(SYS_pipe, (long)pipefd);
}

// sys_dup2: Duplicates a file descriptor, making newfd point to the same file description as oldfd.
// This is critical for redirection (e.g. mapping a file or pipe to standard input (0) or output (1)).
int sys_dup2(int oldfd, int newfd) {
    return syscall2(SYS_dup2, oldfd, newfd);
}

// sys_fork: Creates a new process (child) by duplicating the calling process (parent).
// Returns 0 in the child, and the child's PID in the parent.
pid_t sys_fork(void) {
    return syscall0(SYS_fork);
}

// sys_execve: Executes a new program, completely replacing the current process image.
int sys_execve(const char *filename, char *const argv[], char *const envp[]) {
    return syscall3(SYS_execve, (long)filename, (long)argv, (long)envp);
}

// sys_exit: Terminates the calling process with a given status code.
void sys_exit(int status) {
    syscall1(SYS_exit, status);
    while (1); // Trap to ensure we never return after exit
}

// sys_wait4: Waits for state changes in a child process (e.g. termination).
pid_t sys_wait4(pid_t pid, int *wstatus, int options, void *rusage) {
    return syscall4(SYS_wait4, pid, (long)wstatus, options, (long)rusage);
}

int sys_getcwd(char *buf, size_t size) {
    return syscall2(SYS_getcwd, (long)buf, (long)size);
}

// sys_chdir: Changes the current working directory of the process.
int sys_chdir(const char *path) {
    return syscall1(SYS_chdir, (long)path);
}

int sys_epoll_create1(int flags) {
    return syscall1(SYS_epoll_create1, flags);
}

int sys_epoll_ctl(int epfd, int op, int fd, struct epoll_event *event) {
    return syscall4(SYS_epoll_ctl, epfd, op, fd, (long)event);
}

int sys_epoll_wait(int epfd, struct epoll_event *events, int maxevents, int timeout) {
    return syscall4(SYS_epoll_wait, epfd, (long)events, maxevents, timeout);
}

int sys_getrusage(int who, struct rusage *usage) {
    return syscall2(SYS_getrusage, who, (long)usage);
}

int sys_clock_gettime(int clk_id, struct timespec *tp) {
    return syscall2(SYS_clock_gettime, clk_id, (long)tp);
}
