#include "jobs.h"
#include "memutils.h"
#include "timing.h"

// This helper just ensures that the 'count' value is seen as volatile, so that
// the compiler doesn't optimize this busy-loop out.
static
void __internal_job_wait_until_job_queue_empty(volatile s64 *count) {
    while(*count) {}
}

static
u32 internal_worker_thread(Job_Worker *worker) {
    create_temp_allocator(worker->system->thread_local_temp_space);

    while(!worker->state.compare(JOB_WORKER_Shutting_Down)) {
        worker->state.store(JOB_WORKER_Waiting_For_Job);
        
        //
        // Put this thread to sleep if there are no more immediate jobs to take.
        //
        if(worker->system->job_queue.count == 0) {
            suspend_thread(&worker->thread);
        }

        //
        // If there are no jobs, then don't spend CPU time waiting.
        //
        thread_wait_if_suspended(&worker->thread);
        if(worker->state.compare(JOB_WORKER_Shutting_Down)) break;
        
        //
        // We are awake, which likely means that there are jobs to do.
        // Get the oldest job in the queue.
        //
        Job_Declaration job;

        {
            tmZone("worker_thread_attempt_to_query_job", TM_DEFAULT_COLOR);
            lock(&worker->system->job_queue_mutex);

            if(worker->system->job_queue.count == 0) {
                // Seems like there were jobs to do, but they got snatched by other workers, so go back
                // to sleep.
                unlock(&worker->system->job_queue_mutex);
                continue;
            }

            // This needs to happen before popping the queue, so that get_number_of_incomplete_jobs() does not catch us in the moment between popping the queue to 0 
            // and not officially running this job.
            worker->state.store(JOB_WORKER_Running_Job);
            job = worker->system->job_queue.pop_first();
            unlock(&worker->system->job_queue_mutex);
        }

        //
        // Actually run the job.
        //
        job.procedure_pointer(job.user_pointer);

#if FOUNDATION_DEVELOPER
        ++worker->completed_job_count;
#endif
    }

    destroy_temp_allocator();
    worker->state.store(JOB_WORKER_Shut_Down);

    return 0;
}



void create_job_system(Job_System *system, s64 worker_count, s64 thread_local_temp_space) {
    tmFunction(TM_DEFAULT_COLOR);

    //
    // Initialize the job queue.
    //
    system->job_queue.allocator = Default_Allocator;
    create_mutex(&system->job_queue_mutex);
    
    //
    // Initialize all worker threads.
    // @@Speed: Should probably assign each thread to a unique core...
    //
    system->thread_local_temp_space = thread_local_temp_space;
    system->worker_count = worker_count;
    system->workers = (Job_Worker *) Default_Allocator->allocate(sizeof(Job_Worker) * system->worker_count);

    for(s64 i = 0; i < system->worker_count; ++i) {
        system->workers[i].state.store(JOB_WORKER_Initializing);
        system->workers[i].system = system;

#if FOUNDATION_DEVELOPER
        system->workers[i].completed_job_count = 0;
#endif

        system->workers[i].thread = create_thread((Thread_Entry_Point) internal_worker_thread, &system->workers[i], false); // internal_worker_thread doesn't take 'void *' as user pointer.
        set_thread_name(&system->workers[i].thread, "Worker_Thread");
    }
}

void destroy_job_system(Job_System *system, Job_System_Shutdown_Mode shutdown_mode) {    
    tmFunction(TM_DEFAULT_COLOR);

    //
    // Busy wait until all jobs are started.
    //
    __internal_job_wait_until_job_queue_empty(&system->job_queue.count);

    //
    // Potentially busy wait until all jobs are completed.
    //
    if(shutdown_mode == JOB_SYSTEM_Wait_On_All_Jobs) {
        while(get_number_of_incomplete_jobs(system)) {}
    }

    //
    // Destroy all workers. These might still try to access the job queue!
    //
    for(s64 i = 0; i < system->worker_count; ++i) {
        system->workers[i].state.store(JOB_WORKER_Shutting_Down);

        switch(shutdown_mode) {
        case JOB_SYSTEM_Join_Workers:
        case JOB_SYSTEM_Wait_On_All_Jobs:
            join_thread(&system->workers[i].thread);
            break;

        case JOB_SYSTEM_Kill_Workers:
            kill_thread(&system->workers[i].thread);
            break;    
        }
    }

    Default_Allocator->deallocate(system->workers);
    system->worker_count = 0;

    //
    // Destroy the job queue.
    //
    system->job_queue.clear();
    destroy_mutex(&system->job_queue_mutex);
}

void spawn_job(Job_System *system, Job_Declaration declaration) {
    tmFunction(TM_DEFAULT_COLOR);

    //
    // Add the job declaration to the queue.
    //
    lock(&system->job_queue_mutex);
    system->job_queue.add(declaration);
    unlock(&system->job_queue_mutex);

    //
    // Resume all workers who are currently waiting for a job.
    //
    for(s64 i = 0; i < system->worker_count; ++i) {
        if(system->workers[i].thread.state == THREAD_STATE_Suspended) resume_thread(&system->workers[i].thread);
    }
}

void wait_for_all_jobs(Job_System *system) {
    while(get_number_of_incomplete_jobs(system) > 0) {}
}

s64 get_number_of_incomplete_jobs(Job_System *system) {
    s64 running_jobs = 0;

    for(s64 i = 0; i < system->worker_count; ++i) {
        if(system->workers[i].state.compare(JOB_WORKER_Running_Job) || system->workers[i].state.compare(JOB_WORKER_Initializing)) { // We should not shut down the job system before all threads have been initialized... This might happen if there is only a very small number of very quick jobs.
            ++running_jobs;
        }
    }
    
    return system->job_queue.count + running_jobs;
}
