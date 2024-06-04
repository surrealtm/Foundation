#include "threads.h"

#if FOUNDATION_WIN32
# define WIN32_LEAN_AND_MEAN
# include <windows.h>

struct Thread_Win32_State {
    b8 setup;
    HANDLE handle;
    CRITICAL_SECTION mutex;
    CONDITION_VARIABLE signal;
};

struct Mutex_Win32_State {
    CRITICAL_SECTION handle;
};

static_assert(sizeof(Thread_Win32_State) <= sizeof(Thread::platform_data), "Thread_Win32_State is bigger than expected.");

static_assert(sizeof(Mutex_Win32_State) <= sizeof(Mutex::platform_data), "Mutex_Win32_State is bigger than expected.");

void __thread_internal_cleanup(Thread_Win32_State *state) {
    if(!state->setup) return;
    DeleteCriticalSection(&state->mutex);
    state->handle = INVALID_HANDLE_VALUE;
    state->setup = false;
}


#elif FOUNDATION_LINUX
# include <pthread.h>
# include <unistd.h>

struct Thread_Linux_State {
    b8 setup;
    pthread_t id;          // 8  bytes
    pthread_mutex_t mutex; // 40 bytes
    pthread_cond_t signal; // 48 bytes
};

struct Mutex_Linux_State {
    pthread_mutex_t handle;
};

static_assert(sizeof(Thread_Linux_State) <= sizeof(Thread::platform_data), "Thread_Linux_State is bigger than expected.");

static_assert(sizeof(Mutex_Linux_State) <= sizeof(Mutex::platform_data), "Mutex_Linux_State is bigger than expected.");

void __thread_internal_cleanup(Thread_Linux_State *state) {
    if(!state->setup) return;
    pthread_mutex_destroy(&state->mutex);
    pthread_cond_destroy(&state->signal);
    state->id    = -1;
    state->setup = false;
}

#endif



/* ------------------------------------------------ Thread API ------------------------------------------------ */

Thread create_thread(Thread_Entry_Point procedure, void *user_pointer, b8 start_as_suspended) {
    Thread thread;

#if FOUNDATION_WIN32
    Thread_Win32_State *state = (Thread_Win32_State *) thread.platform_data;
    state->setup = false;
    state->handle = CreateThread(null, 0, procedure, user_pointer, 0, null);

    if(state->handle) {
        InitializeCriticalSection(&state->mutex);
        InitializeConditionVariable(&state->signal);
        state->setup = true;
        thread.state = THREAD_STATE_Running;
    } else {
        thread.state = THREAD_STATE_Terminated; // We should probably give a better error here...
    }
#elif FOUNDATION_LINUX
    Thread_Linux_State *state = (Thread_Linux_State *) thread.platform_data;
    state->setup = false;
    if(pthread_create(&state->id, null, (void *(*)(void*)) procedure, user_pointer) == 0 &&
       pthread_mutex_init(&state->mutex, null) == 0 &&
       pthread_cond_init(&state->signal, null) == 0) {
        state->setup = true;
        thread.state = THREAD_STATE_Running;
    } else {
        thread.state = THREAD_STATE_Terminated;
    }
#endif

    if(start_as_suspended) suspend_thread(&thread); // This is a bit ugly, but the pthread library does not have built-in support for this.
    
    return thread;
}

void join_thread(Thread *thread) {
    if(thread->state == THREAD_STATE_Terminated) return;

    if(thread->state == THREAD_STATE_Suspending || thread->state == THREAD_STATE_Suspended) {
        //
        // The thread must exit gracefully on join, so resume it, and make sure that it
        // actually received the resume signal. If not, we would endlessly wait while it
        // was suspended.
        //
        resume_thread(thread);
    }

#if FOUNDATION_WIN32
    Thread_Win32_State *state = (Thread_Win32_State *) thread->platform_data;
    if(!state->setup) return;

    b8 this_call_destroyed_thread = false;
    
    EnterCriticalSection(&state->mutex);

    // Make sure the thread has received its resuming signal and is now running again. This prevents
    // the issue where we snatch the signal before the thread_wait_if_suspended procedure, in which case
    // the thread would never awaken, and never properly exit.
    while(thread->state == THREAD_STATE_Resuming) {
        SleepConditionVariableCS(&state->signal, &state->mutex, INFINITE);
    }

    if(thread->state != THREAD_STATE_Terminated) {
        // Only attempt to join if the thread is running, and has not since terminated.
        thread->state = THREAD_STATE_Terminating;
        WaitForSingleObject(state->handle, INFINITE);
        CloseHandle(state->handle);
        thread->state = THREAD_STATE_Terminated;
        this_call_destroyed_thread = true;
    }
    
    LeaveCriticalSection(&state->mutex);
    if(this_call_destroyed_thread) __thread_internal_cleanup(state);
#elif FOUNDATION_LINUX
    Thread_Linux_State *state = (Thread_Linux_State *) thread->platform_data;
    if(!state->setup) return;

    b8 this_call_destroyed_thread = false;

    // Make sure the thread has received its resuming signal and is now running again. This prevents
    // the issue where we snatch the signal before the thread_wait_if_suspended procedure, in which case
    // the thread would never awaken, and never properly exit.
    while(thread->state == THREAD_STATE_Resuming) {
        pthread_cond_wait(&state->signal, &state->mutex);
    }

    // Only attempt to join if the thread is running, and has not since terminated.
    if(thread->state != THREAD_STATE_Terminated) {
        thread->state = THREAD_STATE_Terminating;
        pthread_join(state->id, null);
        thread->state = THREAD_STATE_Terminated;
    }

    pthread_mutex_unlock(&state->mutex);
    if(this_call_destroyed_thread) __thread_internal_cleanup(state);
#endif
}

void detach_thread(Thread *thread) {
    if(thread->state == THREAD_STATE_Terminated || thread->state == THREAD_STATE_Terminating) return;

#if FOUNDATION_WIN32
    Thread_Win32_State *state = (Thread_Win32_State *) thread->platform_data;
    if(!state->setup) return;

    b8 this_call_destroyed_thread = true;
    
    EnterCriticalSection(&state->mutex);

    if(thread->state != THREAD_STATE_Terminated && thread->state != THREAD_STATE_Detached) {
        CloseHandle(state->handle);
        thread->state = THREAD_STATE_Detached;
    }

    LeaveCriticalSection(&state->mutex);
    if(this_call_destroyed_thread) __thread_internal_cleanup(state);
#elif FOUNDATION_LINUX
    Thread_Linux_State *state = (Thread_Linux_State *) thread->platform_data;
    if(!state->setup) return;

    b8 this_call_destroyed_thread = true;

    pthread_mutex_lock(&state->mutex);

    if(thread->state != THREAD_STATE_Terminated && thread->state != THREAD_STATE_Detached) {
        pthread_detach(state->id);
        thread->state = THREAD_STATE_Detached;
    }

    pthread_mutex_unlock(&state->mutex);
    if(this_call_destroyed_thread) __thread_internal_cleanup(state);
#endif
}

void suspend_thread(Thread *thread) {
    if(thread->state != THREAD_STATE_Running) return;

#if FOUNDATION_WIN32
    Thread_Win32_State *state = (Thread_Win32_State *) thread->platform_data;
    if(!state->setup) return;

    EnterCriticalSection(&state->mutex);
    if(thread->state == THREAD_STATE_Running) thread->state = THREAD_STATE_Suspending;
    LeaveCriticalSection(&state->mutex);
#elif FOUNDATION_LINUX
    Thread_Linux_State *state = (Thread_Linux_State *) thread->platform_data;
    if(!state->setup) return;

    pthread_mutex_lock(&state->mutex);
    if(thread->state == THREAD_STATE_Running) thread->state = THREAD_STATE_Suspending;
    pthread_mutex_unlock(&state->mutex);
#endif
}

void resume_thread(Thread *thread) {
    if(thread->state != THREAD_STATE_Suspended) return;

#if FOUNDATION_WIN32
    Thread_Win32_State *state = (Thread_Win32_State *) thread->platform_data;
    if(!state->setup) return;

    EnterCriticalSection(&state->mutex);
    if(thread->state == THREAD_STATE_Suspended) thread->state = THREAD_STATE_Resuming;
    WakeAllConditionVariable(&state->signal);
    LeaveCriticalSection(&state->mutex);
#elif FOUNDATION_LINUX
    Thread_Linux_State *state = (Thread_Linux_State *) thread->platform_data;
    if(!state->setup) return;

    pthread_mutex_lock(&state->mutex);
    if(thread->state == THREAD_STATE_Suspended) thread->state = THREAD_STATE_Resuming;
    pthread_cond_broadcast(&state->signal);
    pthread_mutex_unlock(&state->mutex);
#endif
}

void thread_wait_if_suspended(Thread *thread) {
    if(thread->state != THREAD_STATE_Suspended && thread->state != THREAD_STATE_Suspending) return;

#if FOUNDATION_WIN32
    Thread_Win32_State *state = (Thread_Win32_State *) thread->platform_data;
    if(!state->setup) return;

    EnterCriticalSection(&state->mutex);
    if(thread->state == THREAD_STATE_Suspending) thread->state = THREAD_STATE_Suspended;

    while(thread->state == THREAD_STATE_Suspended) {
        SleepConditionVariableCS(&state->signal, &state->mutex, INFINITE);
    }

    thread->state = THREAD_STATE_Running;
    WakeAllConditionVariable(&state->signal); // Signal back that we have resumed (e.g. to the join_thread procedure).
    
    LeaveCriticalSection(&state->mutex);
#elif FOUNDATION_LINUX
    Thread_Linux_State *state = (Thread_Linux_State *) thread->platform_data;
    if(!state->setup) return;

    pthread_mutex_lock(&state->mutex);
    if(thread->state == THREAD_STATE_Suspending) thread->state = THREAD_STATE_Suspended;

    while(thread->state == THREAD_STATE_Suspended) {
        pthread_cond_wait(&state->signal, &state->mutex);
    }

    thread->state = THREAD_STATE_Running;
    pthread_cond_broadcast(&state->signal);
    pthread_mutex_unlock(&state->mutex);
#endif
}

void thread_sleep(f32 seconds) {
#if FOUNDATION_WIN32
    Sleep((DWORD) (seconds * 1000.0f));
#elif FOUNDATION_LINUX
    usleep((useconds_t) (seconds * 1000000.0f));
#endif
}


u32 thread_get_id() {
#if FOUNDATION_WIN32
    return GetCurrentThreadId();
#elif FOUNDATION_LINUX
    return gettid();
#endif
}



/* ------------------------------------------------ Mutex API ------------------------------------------------ */

void create_mutex(Mutex *mutex) {
#if FOUNDATION_WIN32
    Mutex_Win32_State *state = (Mutex_Win32_State *) mutex->platform_data;
    InitializeCriticalSection(&state->handle);
#elif FOUNDATION_LINUX
    Mutex_Linux_State *state = (Mutex_Linux_State *) mutex->platform_data;
    pthread_mutex_init(&state->handle, null);    
#endif
}

void destroy_mutex(Mutex *mutex) {
#if FOUNDATION_WIN32
    Mutex_Win32_State *state = (Mutex_Win32_State *) mutex->platform_data;
    DeleteCriticalSection(&state->handle);
#elif FOUNDATION_LINUX
    Mutex_Linux_State *state = (Mutex_Linux_State *) mutex->platform_data;
    pthread_mutex_destroy(&state->handle);
#endif
}

void lock(Mutex *mutex) {
#if FOUNDATION_WIN32
    Mutex_Win32_State *state = (Mutex_Win32_State *) mutex->platform_data;
    EnterCriticalSection(&state->handle);
#elif FOUNDATION_LINUX    
    Mutex_Linux_State *state = (Mutex_Linux_State *) mutex->platform_data;
    pthread_mutex_lock(&state->handle);
#endif
}

void unlock(Mutex *mutex) {
#if FOUNDATION_WIN32
    Mutex_Win32_State *state = (Mutex_Win32_State *) mutex->platform_data;
    LeaveCriticalSection(&state->handle);
#elif FOUNDATION_LINUX
    Mutex_Linux_State *state = (Mutex_Linux_State *) mutex->platform_data;
    pthread_mutex_unlock(&state->handle);
#endif
}



/* ------------------------------------------------ Atomic API ------------------------------------------------ */

u64 interlocked_compare_exchange(u64 volatile *dst, u64 src, u64 cmp) {
#if FOUNDATION_WIN32
    return InterlockedCompareExchange(dst, src, cmp);
#elif FOUNDATION_LINUX
    return __sync_val_compare_and_swap(dst, src, cmp);
#endif
}
