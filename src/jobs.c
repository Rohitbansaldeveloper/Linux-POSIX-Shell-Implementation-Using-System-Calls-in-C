/*
 * jobs.c
 * 
 * Manages background process tracking. When a command ends with '&', it runs
 * asynchronously. The shell doesn't block waiting for it to finish. However,
 * when it eventually does finish, it becomes a "zombie" process until the shell
 * reads its exit status using waitpid (wait4).
 */

#include "jobs.h"
#include "syscalls.h"
#include "string_utils.h"

void init_jobs(void) {
    // In a full implementation, we might allocate a dynamic list of jobs here
    // to track process IDs, command names, and states (Running/Stopped).
}

/*
 * This function should be called frequently (e.g. before every prompt) to reap
 * any background processes that have terminated.
 */
void check_background_jobs(void) {
    int status;
    pid_t pid;
    
    // sys_wait4 with WNOHANG flag instructs the kernel to return immediately
    // if no child has exited. If a child HAS exited, it returns its PID.
    // We loop with -1 (meaning "wait for ANY child process") until it returns <= 0.
    while ((pid = sys_wait4(-1, &status, WNOHANG, NULL)) > 0) {
        // Output that the job finished.
        // In a real shell, we'd lookup the PID in our job list to print the command name.
        print_str(1, "[Background Job ");
        print_int(1, pid);
        print_str(1, "] completed.\n");
    }
}
