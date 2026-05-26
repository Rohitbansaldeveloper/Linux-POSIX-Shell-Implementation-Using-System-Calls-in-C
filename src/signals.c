/*
 * signals.c
 * 
 * Manages how the shell process reacts to POSIX signals sent by the OS or the user.
 * 
 * Normally, hitting Ctrl+C sends SIGINT to the foreground process group, which
 * would kill the shell. A proper shell ignores SIGINT and SIGQUIT so it can
 * continue running while allowing the child processes (commands) to be killed.
 */

#include "signals.h"
#include "syscalls.h"
#include "memory.h"

// The kernel's internal representation of the sigaction structure for x86_64.
// This differs slightly from the one provided by glibc.
struct k_sigaction {
    void (*sa_handler)(int);  // Pointer to the signal handler function
    unsigned long sa_flags;   // Flags to modify signal behavior
    void (*sa_restorer)(void);// Function to restore context (not needed here)
    unsigned long sa_mask;    // Signal mask to block during handler execution
};

// Special constants for sa_handler
#define SIG_DFL ((void (*)(int))0) // Default action (usually kill process)
#define SIG_IGN ((void (*)(int))1) // Ignore the signal

void setup_signals(void) {
    struct k_sigaction sa;
    
    // We want to ignore the signal entirely
    sa.sa_handler = SIG_IGN;
    sa.sa_flags = 0;
    sa.sa_restorer = 0;
    sa.sa_mask = 0;

    // SYS_rt_sigaction registers the signal handler with the kernel.
    // The last argument (8) is the size of the sigset_t (sa_mask) in bytes.
    // We ignore SIGINT (Ctrl+C).
    syscall4(SYS_rt_sigaction, SIGINT, (long)&sa, 0, 8);
    
    // We ignore SIGQUIT (Ctrl+\).
    syscall4(SYS_rt_sigaction, SIGQUIT, (long)&sa, 0, 8);
    
    /* 
     * Note: When we sys_fork() a child process, the child inherits these signal
     * dispositions. However, because we are using sys_execve() to load a new program,
     * any signal set to SIG_IGN remains ignored, which is actually bad (a child 
     * command like 'cat' couldn't be Ctrl+C'd). 
     * In a fully complete POSIX shell, after forking but before execve, the child 
     * process resets SIGINT and SIGQUIT back to SIG_DFL so they can be interrupted!
     */
}
