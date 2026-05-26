#ifndef JOBS_H
#define JOBS_H

#include "syscalls.h"

#define MAX_JOBS 32

typedef enum {
    JOB_RUNNING,
    JOB_STOPPED,
    JOB_DONE
} JobState;

typedef struct {
    int id; // 1, 2, 3...
    pid_t pgid;
    JobState state;
    char command[256];
} Job;

void init_jobs(void);
void check_background_jobs(void);

// Advanced job control functions
int add_job(pid_t pgid, const char *cmd, JobState state);
void remove_job(pid_t pgid);
Job *get_job(pid_t pgid);
Job *get_job_by_id(int id);
void print_jobs(void);
int continue_job(int id, int background);

#endif
