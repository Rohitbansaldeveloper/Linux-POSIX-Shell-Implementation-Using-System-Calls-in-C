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
    
    // Ignore job control signals in the shell parent so it doesn't get suspended
    // when giving the terminal to child processes
    syscall4(SYS_rt_sigaction, SIGTTOU, (long)&sa, 0, 8);
    syscall4(SYS_rt_sigaction, SIGTTIN, (long)&sa, 0, 8);
    syscall4(SYS_rt_sigaction, SIGTSTP, (long)&sa, 0, 8);
}

void reset_signals(void) {
    struct k_sigaction sa;
    
    // Reset to default action (SIG_DFL)
    sa.sa_handler = SIG_DFL;
    sa.sa_flags = 0;
    sa.sa_restorer = 0;
    sa.sa_mask = 0;

    syscall4(SYS_rt_sigaction, SIGINT, (long)&sa, 0, 8);
    syscall4(SYS_rt_sigaction, SIGQUIT, (long)&sa, 0, 8);
    syscall4(SYS_rt_sigaction, SIGTTOU, (long)&sa, 0, 8);
    syscall4(SYS_rt_sigaction, SIGTTIN, (long)&sa, 0, 8);
    syscall4(SYS_rt_sigaction, SIGTSTP, (long)&sa, 0, 8);
    
    // Unblock SIGCHLD in child process (in case we blocked it for signalfd in parent)
    unsigned long mask = (1UL << (SIGCHLD - 1));
    sys_rt_sigprocmask(SIG_UNBLOCK, &mask, 0, 8);
}

int create_sigchld_fd(void) {
    // We must block SIGCHLD before creating the signalfd
    // A sigset_t is simply a bitmask. For SIGCHLD (17), it's the 16th bit (1 << (17-1)).
    unsigned long mask = (1UL << (SIGCHLD - 1));
    
    // Block SIGCHLD
    sys_rt_sigprocmask(SIG_BLOCK, &mask, 0, 8);
    
    // Create signalfd
    return sys_signalfd4(-1, &mask, 8, SFD_NONBLOCK | SFD_CLOEXEC);
}
