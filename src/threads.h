#pragma once

#include "foundation.h"


/* The threading API is supposed to make multithreading platform independent and a bit easier. It gives access to
 * threads and mutexes.
 * Threads can be created and then either joined or detached. This module also supports suspending threads on
 * a user level (which can be really useful for e.g. a job system). This means that any thread can mark another
 * as suspended, but this thread can decide when (and if) to wait until it is resumed in user code (see
 * thread_wait_if_suspended). Until then, the thread is still considered to be running.
 * This module also supports basic mutexes.
 */



/* ------------------------------------------------ Thread API ------------------------------------------------ */

typedef u32(*Thread_Entry_Point)(void*);

#define THREAD_INTERNAL_STATE_SIZE 64 // This is the highest size of internal platform data needed to be stored, to avoid a memory allocation here (and to avoid platform headers here...)

enum Thread_State {
    THREAD_STATE_Running     = 0x0,
    THREAD_STATE_Suspending  = 0x1,
    THREAD_STATE_Suspended   = 0x2,
    THREAD_STATE_Resuming    = 0x3,
    THREAD_STATE_Terminating = 0x4,
    THREAD_STATE_Terminated  = 0x5,
    THREAD_STATE_Detached    = 0x6,
};

struct Thread {
    u8 platform_data[THREAD_INTERNAL_STATE_SIZE];
    Thread_State state;        
};


Thread create_thread(Thread_Entry_Point procedure, void *user_pointer, b8 start_as_suspended);
void join_thread(Thread *thread);
void detach_thread(Thread *thread);
void suspend_thread(Thread *thread);
void resume_thread(Thread *thread);
void thread_wait_if_suspended(Thread *thread);

u32 thread_get_id();
void thread_sleep(f32 seconds);


/* ------------------------------------------------ Mutex API ------------------------------------------------ */

#define MUTEX_INTERNAL_STATE_SIZE 40

struct Mutex {
    u8 platform_data[MUTEX_INTERNAL_STATE_SIZE];
};

void create_mutex(Mutex *mutex);
void destroy_mutex(Mutex *mutex);
void lock(Mutex *mutex);
void unlock(Mutex *mutex);



/* ------------------------------------------------ Atomic API ------------------------------------------------ */

u64 interlocked_compare_exchange(u64 volatile *dst, u64 src, u64 cmp);
