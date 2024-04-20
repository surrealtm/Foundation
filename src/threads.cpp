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

static_assert(sizeof(Thread_Win32_State) <= sizeof(Thread::platform_data), "Thread_Win32_State is bigger than expected.");

void __thread_internal_cleanup(Thread_Win32_State *win32) {
    if(!win32->setup) return;
    DeleteCriticalSection(&win32->mutex);
    win32->handle = INVALID_HANDLE_VALUE;
    win32->setup = false;
}


struct Mutex_Win32_State {
    CRITICAL_SECTION handle;
};

static_assert(sizeof(Mutex_Win32_State) <= sizeof(Mutex::platform_data), "Mutex_Win32_State is bigger than expected.");

#endif



/* ------------------------------------------------ Thread API ------------------------------------------------ */

Thread create_thread(Thread_Entry_Point procedure, void *user_pointer, b8 start_as_suspended) {
    Thread thread;

#if FOUNDATION_WIN32
    Thread_Win32_State *win32 = (Thread_Win32_State *) thread.platform_data;
    win32->setup = false;
    win32->handle = CreateThread(null, 0, procedure, user_pointer, 0, null);

    if(win32->handle) {
        InitializeCriticalSection(&win32->mutex);
        InitializeConditionVariable(&win32->signal);
        win32->setup = true;
        thread.state = THREAD_STATE_Running;
    } else {
        thread.state = THREAD_STATE_Running; // We should probably give a better error here...
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
    Thread_Win32_State *win32 = (Thread_Win32_State *) thread->platform_data;
    if(!win32->setup) return;

    b8 this_call_destroyed_thread = false;
    
    EnterCriticalSection(&win32->mutex);

    // Make sure the thread has received its resuming signal and is now running again. This prevents
    // the issue where we snatch the signal before the thread_wait_if_suspended procedure, in which case
    // the thread would never awaken, and never properly exit.
    while(thread->state == THREAD_STATE_Resuming) {
        SleepConditionVariableCS(&win32->signal, &win32->mutex, INFINITE);
    }

    if(thread->state != THREAD_STATE_Terminated) {
        // Only attempt to join if the thread is running, and has not since terminated.
        thread->state = THREAD_STATE_Terminating;
        WaitForSingleObject(win32->handle, INFINITE);
        CloseHandle(win32->handle);
        thread->state = THREAD_STATE_Terminated;
        this_call_destroyed_thread = true;
    }
    
    LeaveCriticalSection(&win32->mutex);
    if(this_call_destroyed_thread) __thread_internal_cleanup(win32);
#endif
}

void detach_thread(Thread *thread) {
    if(thread->state == THREAD_STATE_Terminated || thread->state == THREAD_STATE_Terminating) return;

#if FOUNDATION_WIN32
    Thread_Win32_State *win32 = (Thread_Win32_State *) thread->platform_data;
    if(!win32->setup) return;

    b8 this_call_destroyed_thread = true;
    
    EnterCriticalSection(&win32->mutex);

    if(thread->state != THREAD_STATE_Terminated && thread->state != THREAD_STATE_Detached) {
        CloseHandle(win32->handle);
        thread->state = THREAD_STATE_Detached;
    }

    LeaveCriticalSection(&win32->mutex);
    if(this_call_destroyed_thread) __thread_internal_cleanup(win32);
#endif
}

void suspend_thread(Thread *thread) {
    if(thread->state != THREAD_STATE_Running) return;

#if FOUNDATION_WIN32
    Thread_Win32_State *win32 = (Thread_Win32_State *) thread->platform_data;
    if(!win32->setup) return;

    EnterCriticalSection(&win32->mutex);
    thread->state = THREAD_STATE_Suspending;
    LeaveCriticalSection(&win32->mutex);
#endif
}

void resume_thread(Thread *thread) {
    if(thread->state != THREAD_STATE_Suspended) return;

#if FOUNDATION_WIN32
    Thread_Win32_State *win32 = (Thread_Win32_State *) thread->platform_data;
    if(!win32->setup) return;

    EnterCriticalSection(&win32->mutex);
    thread->state = THREAD_STATE_Resuming;
    WakeAllConditionVariable(&win32->signal);
    LeaveCriticalSection(&win32->mutex);
#endif
}

void thread_wait_if_suspended(Thread *thread) {
    if(thread->state != THREAD_STATE_Suspended && thread->state != THREAD_STATE_Suspending) return;

#if FOUNDATION_WIN32
    Thread_Win32_State *win32 = (Thread_Win32_State *) thread->platform_data;
    if(!win32->setup) return;

    EnterCriticalSection(&win32->mutex);
    if(thread->state == THREAD_STATE_Suspending) thread->state = THREAD_STATE_Suspended;

    while(thread->state == THREAD_STATE_Suspended) {
        SleepConditionVariableCS(&win32->signal, &win32->mutex, INFINITE);
    }

    thread->state = THREAD_STATE_Running;
    WakeAllConditionVariable(&win32->signal); // Signal back that we have resumed (e.g. to the join_thread procedure).
    
    LeaveCriticalSection(&win32->mutex);
#endif
}

void thread_sleep(f32 seconds) {
#if FOUNDATION_WIN32
    Sleep((DWORD) (seconds * 1000.0f));
#endif
}


u32 thread_get_id() {
#if FOUNDATION_WIN32
    return GetCurrentThreadId();
#endif
}



/* ------------------------------------------------ Mutex API ------------------------------------------------ */

void create_mutex(Mutex *mutex) {
#if FOUNDATION_WIN32
    Mutex_Win32_State *win32 = (Mutex_Win32_State *) mutex->platform_data;
    InitializeCriticalSection(&win32->handle);
#endif
}

void destroy_mutex(Mutex *mutex) {
#if FOUNDATION_WIN32
    Mutex_Win32_State *win32 = (Mutex_Win32_State *) mutex->platform_data;
    DeleteCriticalSection(&win32->handle);
#endif
}

void lock(Mutex *mutex) {
#if FOUNDATION_WIN32
    Mutex_Win32_State *win32 = (Mutex_Win32_State *) mutex->platform_data;
    EnterCriticalSection(&win32->handle);
#endif
}

void unlock(Mutex *mutex) {
#if FOUNDATION_WIN32
    Mutex_Win32_State *win32 = (Mutex_Win32_State *) mutex->platform_data;
    LeaveCriticalSection(&win32->handle);
#endif
}



/* ------------------------------------------------ Atomic API ------------------------------------------------ */

u64 interlocked_compare_exchange(u64 volatile *dst, u64 src, u64 cmp) {
#if FOUNDATION_WIN32
    return InterlockedCompareExchange(dst, src, cmp);
#endif
}
