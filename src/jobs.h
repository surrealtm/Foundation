#pragma once

#include "foundation.h"
#include "threads.h"
#include "memutils.h"

struct Job_System;

typedef void(*Job_Procedure)(void *);

enum Job_System_Shutdown_Mode {
    JOB_SYSTEM_Wait_On_All_Jobs, // Only shut down after all declared jobs have been completed.
    JOB_SYSTEM_Join_Workers,     // Wait until all currently running jobs are completed, but don't start running new ones.
    JOB_SYSTEM_Kill_Workers,     // Try to forcefully kill the worker threads.
    // There is no mode for detaching the workers as that would surely crash, since we deallocate the Job_Worker array leading to an Access Violation.
};

enum Job_Worker_State {
    JOB_WORKER_Initializing,
    JOB_WORKER_Waiting_For_Job,
    JOB_WORKER_Running_Job,
    JOB_WORKER_Shutting_Down,
    JOB_WORKER_Shut_Down,
};

struct Job_Declaration {
    Job_Procedure procedure_pointer;
    void *user_pointer;
};

struct Job_Worker {
    Atomic<u32> state; // Job_Worker_State
    Job_System *system;
    Thread thread;

#if FOUNDATION_DEVELOPER
    s64 completed_job_count;
#endif
};

struct Job_System {
    s64 thread_local_temp_space;
    
    s64 worker_count;
    Job_Worker *workers;

    Mutex job_queue_mutex;
    Resizable_Array<Job_Declaration> job_queue;
};

void create_job_system(Job_System *system, s64 worker_count, s64 thread_local_temp_space = 64 * ONE_MEGABYTE);
void destroy_job_system(Job_System *system, Job_System_Shutdown_Mode shutdown_mode);
void spawn_job(Job_System *system, Job_Declaration declaration);
void wait_for_all_jobs(Job_System *system);
s64 get_number_of_incomplete_jobs(Job_System *system);
