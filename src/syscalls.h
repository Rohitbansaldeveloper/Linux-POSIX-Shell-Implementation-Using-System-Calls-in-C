#ifndef SYSCALLS_H
#define SYSCALLS_H

typedef unsigned long size_t;
typedef long ssize_t;
typedef int pid_t;

#define SYS_read 0
#define SYS_write 1
#define SYS_open 2
#define SYS_close 3
#define SYS_mmap 9
#define SYS_munmap 11
#define SYS_brk 12
#define SYS_rt_sigaction 13
#define SYS_ioctl 16
#define SYS_pipe 22
#define SYS_dup2 33
#define SYS_getpid 39
#define SYS_fork 57
#define SYS_execve 59
#define SYS_exit 60
#define SYS_wait4 61
#define SYS_kill 62
#define SYS_getcwd 79
#define SYS_chdir 80
#define SYS_setpgid 109
#define SYS_getpgrp 111
#define SYS_getdents64 217

long syscall0(long n);
long syscall1(long n, long a1);
long syscall2(long n, long a1, long a2);
long syscall3(long n, long a1, long a2, long a3);
long syscall4(long n, long a1, long a2, long a3, long a4);
long syscall6(long n, long a1, long a2, long a3, long a4, long a5, long a6);

ssize_t sys_read(int fd, void *buf, size_t count);
ssize_t sys_write(int fd, const void *buf, size_t count);
int sys_open(const char *filename, int flags, int mode);
int sys_close(int fd);
void *sys_mmap(void *addr, size_t length, int prot, int flags, int fd, long offset);
int sys_munmap(void *addr, size_t length);
void *sys_brk(void *addr);
int sys_ioctl(int fd, unsigned long request, void *argp);
int sys_pipe(int pipefd[2]);
int sys_dup2(int oldfd, int newfd);
pid_t sys_fork(void);
int sys_execve(const char *filename, char *const argv[], char *const envp[]);
void sys_exit(int status);
pid_t sys_wait4(pid_t pid, int *wstatus, int options, void *rusage);
int sys_getcwd(char *buf, size_t size);
int sys_chdir(const char *path);
int sys_kill(pid_t pid, int sig);
int sys_getdents64(unsigned int fd, void *dirp, unsigned int count);
int sys_setpgid(pid_t pid, pid_t pgid);
pid_t sys_getpgrp(void);
pid_t sys_getpid(void);

/* Signal types */
#define SIGINT   2
#define SIGQUIT  3
#define SIGKILL  9
#define SIGCHLD  17
#define SIGCONT  18
#define SIGSTOP  19
#define SIGTSTP  20
#define SIGTTIN  21
#define SIGTTOU  22

/* sigaction structures and flags */
#define WNOHANG 1
#define WUNTRACED 2

/* Macros for inspecting wait status */
#define WTERMSIG(status)    ((status) & 0x7f)
#define WIFEXITED(status)   (WTERMSIG(status) == 0)
#define WEXITSTATUS(status) (((status) & 0xff00) >> 8)
#define WIFSIGNALED(status) (((signed char) (((status) & 0x7f) + 1) >> 1) > 0)
#define WIFSTOPPED(status)  (((status) & 0xff) == 0x7f)

#endif
