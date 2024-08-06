#include "threads.h"

#if FOUNDATION_WIN32
# define WIN32_LEAN_AND_MEAN
# include <windows.h>
# include <intrin.h>

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
    if(thread->state == THREAD_STATE_Terminated || thread->state == THREAD_STATE_Detached) return;

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
        this_call_destroyed_thread = true;
    }

    pthread_mutex_unlock(&state->mutex);
    if(this_call_destroyed_thread) __thread_internal_cleanup(state);
#endif
}

void detach_thread(Thread *thread) {
    if(thread->state == THREAD_STATE_Terminated || thread->state == THREAD_STATE_Terminating || thread->state == THREAD_STATE_Detached) return;

#if FOUNDATION_WIN32
    Thread_Win32_State *state = (Thread_Win32_State *) thread->platform_data;
    if(!state->setup) return;

    b8 this_call_destroyed_thread = false;
    
    EnterCriticalSection(&state->mutex);

    if(thread->state != THREAD_STATE_Terminated && thread->state != THREAD_STATE_Detached) {
        CloseHandle(state->handle);
        thread->state = THREAD_STATE_Detached;
        this_call_destroyed_thread = true;
    }

    LeaveCriticalSection(&state->mutex);
    if(this_call_destroyed_thread) __thread_internal_cleanup(state);
#elif FOUNDATION_LINUX
    Thread_Linux_State *state = (Thread_Linux_State *) thread->platform_data;
    if(!state->setup) return;

    b8 this_call_destroyed_thread = false;

    pthread_mutex_lock(&state->mutex);

    if(thread->state != THREAD_STATE_Terminated && thread->state != THREAD_STATE_Detached) {
        pthread_detach(state->id);
        thread->state = THREAD_STATE_Detached;
        this_call_destroyed_thread = true;
    }

    pthread_mutex_unlock(&state->mutex);
    if(this_call_destroyed_thread) __thread_internal_cleanup(state);
#endif
}

void kill_thread(Thread *thread) {
    if(thread->state == THREAD_STATE_Terminated || thread->state == THREAD_STATE_Terminating || thread->state == THREAD_STATE_Detached) return;

#if FOUNDATION_WIN32
    Thread_Win32_State *state = (Thread_Win32_State *) thread->platform_data;
    if(!state->setup) return;
    
    b8 this_call_destroyed_thread = false;
    
    EnterCriticalSection(&state->mutex);
    
    if(thread->state != THREAD_STATE_Terminated && thread->state != THREAD_STATE_Detached) {
        thread->state = THREAD_STATE_Terminating;
        TerminateThread(state->handle, 0);
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

    pthread_mutex_lock(&state->mutex);

    if(thread->state != THREAD_STATE_Terminated && thread->state != THREAD_STATE_Detached) {
        thread->state = THREAD_STATE_Terminating;
        pthread_cancel(state->id);
        pthread_join(state->id);
        thread->state = THREAD_STATE_Terminated;
        this_call_destroyed_thread = true;
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



/* ---------------------------------------------- Semaphose API ---------------------------------------------- */

void create_semaphore(Semaphore *semaphore) {
    semaphore->counter = 0;
}

void destroy_semaphore(Semaphore *semaphore) {
}

void lock_shared(Semaphore *semaphore) {
    do {
        if(semaphore->counter == -1) continue; // Exclusive lock, wait.

        if(atomic_compare_exchange(&semaphore->counter, semaphore->counter + 1, semaphore->counter) >= 0) break;
    } while(true);
}

void unlock_shared(Semaphore *semaphore) {
    atomic_exchange_add(&semaphore->counter, -1);
}

void lock_exclusive(Semaphore *semaphore) {
    while(atomic_compare_exchange(&semaphore->counter, -1, 0) != 0) {};
}

void unlock_exclusive(Semaphore *semaphore) {
    atomic_store(&semaphore->counter, 0);
}



/* ------------------------------------------------ Atomic API ------------------------------------------------ */

u64 atomic_compare_exchange(u64 volatile *dst, u64 desired, u64 expected) {
#if FOUNDATION_WIN32
    return _InterlockedCompareExchange64((LONG64 volatile *) dst, desired, expected);
#elif FOUNDATION_LINUX
    return __sync_val_compare_and_swap(dst, desired, expected);
#endif
}

u32 atomic_compare_exchange(u32 volatile *dst, u32 desired, u32 expected) {
#if FOUNDATION_WIN32
    return _InterlockedCompareExchange((LONG volatile *) dst, desired, expected);
#elif FOUNDATION_LINUX
    return __sync_val_compare_and_swap(dst, desired, expected);
#endif
}

u16 atomic_compare_exchange(u16 volatile *dst, u16 desired, u16 expected) {
#if FOUNDATION_WIN32
    return _InterlockedCompareExchange16((SHORT volatile *) dst, desired, expected);
#elif FOUNDATION_LINUX
    return __sync_val_compare_and_swap(dst, desired, expected);
#endif
}

u8 atomic_compare_exchange(u8 volatile *dst, u8 desired, u8 expected) {
#if FOUNDATION_WIN32
    return _InterlockedCompareExchange8((CHAR volatile *) dst, desired, expected);
#elif FOUNDATION_LINUX
    return __sync_val_compare_and_swap(dst, desired, expected);
#endif
}

s64 atomic_compare_exchange(s64 volatile *dst, s64 desired, s64 expected) {
#if FOUNDATION_WIN32
    return _InterlockedCompareExchange64((LONG64 volatile *) dst, desired, expected);
#elif FOUNDATION_LINSX
    return __sync_val_compare_and_swap(dst, desired, expected);
#endif
}

s32 atomic_compare_exchange(s32 volatile *dst, s32 desired, s32 expected) {
#if FOUNDATION_WIN32
    return _InterlockedCompareExchange((LONG volatile *) dst, desired, expected);
#elif FOUNDATION_LINSX
    return __sync_val_compare_and_swap(dst, desired, expected);
#endif
}

s16 atomic_compare_exchange(s16 volatile *dst, s16 desired, s16 expected) {
#if FOUNDATION_WIN32
    return _InterlockedCompareExchange16((SHORT volatile *) dst, desired, expected);
#elif FOUNDATION_LINSX
    return __sync_val_compare_and_swap(dst, desired, expected);
#endif
}

s8 atomic_compare_exchange(s8 volatile *dst, s8 desired, s8 expected) {
#if FOUNDATION_WIN32
    return _InterlockedCompareExchange8((CHAR volatile *) dst, desired, expected);
#elif FOUNDATION_LINSX
    return __sync_val_compare_and_swap(dst, desired, expected);
#endif
}



u64 atomic_load(u64 volatile *value) {
#if FOUNDATION_WIN32
    return _InterlockedOr64((LONG64 volatile *) value, 0);
#elif FOUNDATION_LINUX
    return __atomic_load_n(value, __ATOMIC_SEQ_CST);
#endif
}

u32 atomic_load(u32 volatile *value) {
#if FOUNDATION_WIN32
    return _InterlockedOr((LONG volatile *) value, 0);
#elif FOUNDATION_LINUX
    return __atomic_load_n(value, __ATOMIC_SEQ_CST);
#endif
}

u16 atomic_load(u16 volatile *value) {
#if FOUNDATION_WIN32
    return _InterlockedOr16((SHORT volatile *) value, 0);
#elif FOUNDATION_LINUX
    return __atomic_load_n(value, __ATOMIC_SEQ_CST);
#endif
}

u8 atomic_load(u8 volatile *value) {
#if FOUNDATION_WIN32
    return _InterlockedOr8((CHAR volatile *) value, 0);
#elif FOUNDATION_LINUX
    return __atomic_load_n(value, __ATOMIC_SEQ_CST);
#endif
}

s64 atomic_load(s64 volatile *value) {
#if FOUNDATION_WIN32
    return _InterlockedOr64((LONG64 volatile *) value, 0);
#elif FOUNDATION_LINUX
    return __atomic_load_n(value, __ATOMIC_SEQ_CST);
#endif
}

s32 atomic_load(s32 volatile *value) {
#if FOUNDATION_WIN32
    return _InterlockedOr((LONG volatile *) value, 0);
#elif FOUNDATION_LINUX
    return __atomic_load_n(value, __ATOMIC_SEQ_CST);
#endif
}

s16 atomic_load(s16 volatile *value) {
#if FOUNDATION_WIN32
    return _InterlockedOr16((SHORT volatile *) value, 0);
#elif FOUNDATION_LINUX
    return __atomic_load_n(value, __ATOMIC_SEQ_CST);
#endif
}

s8 atomic_load(s8 volatile *value) {
#if FOUNDATION_WIN32
    return _InterlockedOr8((CHAR volatile *) value, 0);
#elif FOUNDATION_LINUX
    return __atomic_load_n(value, __ATOMIC_SEQ_CST);
#endif
}



void atomic_store(u64 volatile *dst, u64 src) {
#if FOUNDATION_WIN32
    _InterlockedExchange64((LONG64 volatile *) dst, src);
#elif FOUNDATION_LINUX
    __atomic_store_n(dst, src, __ATOMIC_SEQ_CST);
#endif
}

void atomic_store(u32 volatile *dst, u32 src) {
#if FOUNDATION_WIN32
    _InterlockedExchange((LONG volatile *) dst, src);
#elif FOUNDATION_LINUX
    __atomic_store_n(dst, src, __ATOMIC_SEQ_CST);
#endif
}

void atomic_store(u16 volatile *dst, u16 src) {
#if FOUNDATION_WIN32
    _InterlockedExchange16((SHORT volatile *) dst, src);
#elif FOUNDATION_LINUX
    __atomic_store_n(dst, src, __ATOMIC_SEQ_CST);
#endif
}

void atomic_store(u8 volatile *dst, u8 src) {
#if FOUNDATION_WIN32
    _InterlockedExchange8((CHAR volatile *) dst, src);
#elif FOUNDATION_LINUX
    __atomic_store_n(dst, src, __ATOMIC_SEQ_CST);
#endif
}

void atomic_store(s64 volatile *dst, s64 src) {
#if FOUNDATION_WIN32
    _InterlockedExchange64((LONG64 volatile *) dst, src);
#elif FOUNDATION_LINUX
    __atomic_store_n(dst, src, __ATOMIC_SEQ_CST);
#endif
}

void atomic_store(s32 volatile *dst, s32 src) {
#if FOUNDATION_WIN32
    _InterlockedExchange((LONG volatile *) dst, src);
#elif FOUNDATION_LINUX
    __atomic_store_n(dst, src, __ATOMIC_SEQ_CST);
#endif
}

void atomic_store(s16 volatile *dst, s16 src) {
#if FOUNDATION_WIN32
    _InterlockedExchange16((SHORT volatile *) dst, src);
#elif FOUNDATION_LINUX
    __atomic_store_n(dst, src, __ATOMIC_SEQ_CST);
#endif
}

void atomic_store(s8 volatile *dst, s8 src) {
#if FOUNDATION_WIN32
    _InterlockedExchange8((CHAR volatile *) dst, src);
#elif FOUNDATION_LINUX
    __atomic_store_n(dst, src, __ATOMIC_SEQ_CST);
#endif
}



void atomic_exchange_add(u64 volatile *dst, u64 src) {
#if FOUNDATION_WIN32
    _InterlockedExchangeAdd64((LONG64 volatile *) dst, src);
#elif FOUNDATION_LINUX
    return __atomic_fetch_add(dst, src, __ATOMIC_SEQ_CST);
#endif
}

void atomic_exchange_add(u32 volatile *dst, u32 src) {
#if FOUNDATION_WIN32
    _InterlockedExchangeAdd((LONG volatile *) dst, src);
#elif FOUNDATION_LINUX
    return __atomic_fetch_add(dst, src, __ATOMIC_SEQ_CST);
#endif
}

void atomic_exchange_add(u16 volatile *dst, u16 src) {
#if FOUNDATION_WIN32
    _InterlockedExchangeAdd16((SHORT volatile *) dst, src);
#elif FOUNDATION_LINUX
    return __atomic_fetch_add(dst, src, __ATOMIC_SEQ_CST);
#endif
}

void atomic_exchange_add(u8 volatile *dst, u8 src) {
#if FOUNDATION_WIN32
    _InterlockedExchangeAdd8((CHAR volatile *) dst, src);
#elif FOUNDATION_LINUX
    return __atomic_fetch_add(dst, src, __ATOMIC_SEQ_CST);
#endif
}

void atomic_exchange_add(s64 volatile *dst, s64 src) {
#if FOUNDATION_WIN32
    _InterlockedExchangeAdd64((LONG64 volatile *) dst, src);
#elif FOUNDATION_LINUX
    return __atomic_fetch_add(dst, src, __ATOMIC_SEQ_CST);
#endif
}

void atomic_exchange_add(s32 volatile *dst, s32 src) {
#if FOUNDATION_WIN32
    _InterlockedExchangeAdd((LONG volatile *) dst, src);
#elif FOUNDATION_LINUX
    return __atomic_fetch_add(dst, src, __ATOMIC_SEQ_CST);
#endif
}

void atomic_exchange_add(s16 volatile *dst, s16 src) {
#if FOUNDATION_WIN32
    _InterlockedExchangeAdd16((SHORT volatile *) dst, src);
#elif FOUNDATION_LINUX
    return __atomic_fetch_add(dst, src, __ATOMIC_SEQ_CST);
#endif
}

void atomic_exchange_add(s8 volatile *dst, s8 src) {
#if FOUNDATION_WIN32
    _InterlockedExchangeAdd8((CHAR volatile *) dst, src);
#elif FOUNDATION_LINUX
    return __atomic_fetch_add(dst, src, __ATOMIC_SEQ_CST);
#endif
}
