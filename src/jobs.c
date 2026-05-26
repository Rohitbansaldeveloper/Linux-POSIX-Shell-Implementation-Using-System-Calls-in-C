/*
 * jobs.c
 * 
 * Manages background process tracking and Job Control.
 */

#include "jobs.h"
#include "syscalls.h"
#include "string_utils.h"
#include "terminal.h"

static Job job_list[MAX_JOBS];
static int next_job_id = 1;

void init_jobs(void) {
    for (int i = 0; i < MAX_JOBS; i++) {
        job_list[i].id = 0;
    }
}

int add_job(pid_t pgid, const char *cmd, JobState state) {
    for (int i = 0; i < MAX_JOBS; i++) {
        if (job_list[i].id == 0) {
            job_list[i].id = next_job_id++;
            job_list[i].pgid = pgid;
            job_list[i].state = state;
            str_cpy(job_list[i].command, cmd);
            return job_list[i].id;
        }
    }
    return -1; // No room
}

void remove_job(pid_t pgid) {
    for (int i = 0; i < MAX_JOBS; i++) {
        if (job_list[i].pgid == pgid) {
            job_list[i].id = 0;
            break;
        }
    }
}

Job *get_job(pid_t pgid) {
    for (int i = 0; i < MAX_JOBS; i++) {
        if (job_list[i].id != 0 && job_list[i].pgid == pgid) {
            return &job_list[i];
        }
    }
    return NULL;
}

Job *get_job_by_id(int id) {
    for (int i = 0; i < MAX_JOBS; i++) {
        if (job_list[i].id == id) {
            return &job_list[i];
        }
    }
    return NULL;
}

void print_jobs(void) {
    for (int i = 0; i < MAX_JOBS; i++) {
        if (job_list[i].id != 0) {
            print_str(1, "[");
            print_int(1, job_list[i].id);
            print_str(1, "] ");
            if (job_list[i].state == JOB_RUNNING) {
                print_str(1, "Running\t\t");
            } else if (job_list[i].state == JOB_STOPPED) {
                print_str(1, "Stopped\t\t");
            } else {
                print_str(1, "Done\t\t");
            }
            print_str(1, job_list[i].command);
            print_str(1, "\n");
        }
    }
}

int continue_job(int id, int background) {
    Job *j = get_job_by_id(id);
    if (!j) return -1;
    
    j->state = JOB_RUNNING;
    print_str(1, j->command);
    print_str(1, "\n");
    
    if (!background) {
        // Put in foreground
        set_foreground_pgrp(0, j->pgid);
    }
    
    // Send SIGCONT
    sys_kill(-j->pgid, SIGCONT);
    
    if (!background) {
        // Wait for it
        int status;
        pid_t p = sys_wait4(-j->pgid, &status, WUNTRACED, NULL);
        
        // Restore shell to foreground
        set_foreground_pgrp(0, sys_getpgrp());
        
        if (p > 0) {
            if (WIFSTOPPED(status)) {
                j->state = JOB_STOPPED;
                print_str(1, "\n["); print_int(1, j->id); print_str(1, "] Stopped\n");
            } else {
                remove_job(j->pgid);
            }
        }
    }
    return 0;
}

void check_background_jobs(void) {
    int status;
    pid_t pid;
    
    // Check all jobs using WNOHANG and WUNTRACED
    // We pass -1 to wait for ANY child.
    while ((pid = sys_wait4(-1, &status, WNOHANG | WUNTRACED, NULL)) > 0) {
        // Find which job this pid belongs to
        // Note: In pipelines, the pgid is the pid of the first process.
        // For simplicity, we assume we just check the pgid of the exiting process.
        pid_t pgid = sys_getpgrp(); // Wait, no, we need the child's pgid.
        // Actually, without a full process map, we'll just try to find a job with pgid == pid
        Job *j = get_job(pid); 
        if (j) {
            if (WIFSTOPPED(status)) {
                if (j->state != JOB_STOPPED) {
                    j->state = JOB_STOPPED;
                    print_str(1, "\n["); print_int(1, j->id); print_str(1, "] Stopped\n");
                }
            } else if (WIFEXITED(status) || WIFSIGNALED(status)) {
                j->state = JOB_DONE;
                print_str(1, "\n["); print_int(1, j->id); print_str(1, "] Done\t\t");
                print_str(1, j->command);
                print_str(1, "\n");
                remove_job(j->pgid);
            }
        }
    }
}
