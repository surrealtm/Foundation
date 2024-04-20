#pragma once

#include "foundation.h"
#include "threads.h"
#include "memutils.h"

struct Job_System;

typedef void(*Job_Procedure)(void *);

enum Job_System_Shutdown_Mode {
    JOB_SYSTEM_Wait_On_All_Jobs, // Only shut down after all declared jobs have been completed.
    JOB_SYSTEM_Join_Workers,     // Wait until all currently running jobs are completed, but don't start running new ones.
    JOB_SYSTEM_Detach_Workers,   // Just detach the worker threads. The worker threads will complete their current jobs, but won't start running new ones.
};

struct Job_Declaration {
    Job_Procedure procedure_pointer;
    void *user_pointer;
};

struct Job_Worker {
    Job_System *system;
    Thread thread;
    b8 shutting_down;
};

struct Job_System {
    s64 worker_count;
    Job_Worker *workers;

    Mutex job_queue_mutex;
    Resizable_Array<Job_Declaration> job_queue;
};

void create_job_system(Job_System *system, s64 worker_count);
void destroy_job_system(Job_System *system, Job_System_Shutdown_Mode shutdown_mode);
void spawn_job(Job_System *system, Job_Declaration declaration);
