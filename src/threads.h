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

#define THREAD_INTERNAL_STATE_SIZE 104 // This is the highest size of internal platform data needed to be stored, to avoid a memory allocation here (and to avoid platform headers here...)

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
    Thread_State state = THREAD_STATE_Detached;
};


Thread create_thread(Thread_Entry_Point procedure, void *user_pointer, b8 start_as_suspended);
void join_thread(Thread *thread);
void detach_thread(Thread *thread);
void suspend_thread(Thread *thread);
void resume_thread(Thread *thread);
void thread_wait_if_suspended(Thread *thread);
void thread_sleep(f32 seconds);
u32 thread_get_id();



/* ------------------------------------------------ Mutex API ------------------------------------------------ */

#define MUTEX_INTERNAL_STATE_SIZE 40

struct Mutex {
    u8 platform_data[MUTEX_INTERNAL_STATE_SIZE];
};

void create_mutex(Mutex *mutex);
void destroy_mutex(Mutex *mutex);
void lock(Mutex *mutex);
void unlock(Mutex *mutex);



/* ---------------------------------------------- Semaphore API ---------------------------------------------- */

struct Semaphore {
    u64 counter;
};

void create_semaphore(Semaphore *semaphore);
void destroy_semaphore(Semaphore *semaphore);
void lock_shared(Semaphore *semaphore);
void unlock_shared(Semaphore *semaphore);
void lock_exclusive(Semaphore *semaphore);
void unlock_exclusive(Semaphore *semaphore);



/* ------------------------------------------------ Atomic API ------------------------------------------------ */

u64 atomic_compare_exchange(u64 volatile *dst, u64 desired, u64 expected);
u32 atomic_compare_exchange(u32 volatile *dst, u32 desired, u32 expected);
u16 atomic_compare_exchange(u16 volatile *dst, u16 desired, u16 expected);
u8  atomic_compare_exchange(u8  volatile *dst, u8  desired, u8  expected);
s64 atomic_compare_exchange(s64 volatile *dst, s64 desired, s64 expected);
s32 atomic_compare_exchange(s32 volatile *dst, s32 desired, s32 expected);
s16 atomic_compare_exchange(s16 volatile *dst, s16 desired, s16 expected);
s8  atomic_compare_exchange(s8  volatile *dst, s8  desired, s8  expected);

u64 atomic_load(u64 volatile *value);
u32 atomic_load(u32 volatile *value);
u16 atomic_load(u16 volatile *value);
u8  atomic_load(u8  volatile *value);
s64 atomic_load(s64 volatile *value);
s32 atomic_load(s32 volatile *value);
s16 atomic_load(s16 volatile *value);
s8  atomic_load(s8  volatile *value);

void atomic_store(u64 volatile *dst, u64 src);
void atomic_store(u32 volatile *dst, u32 src);
void atomic_store(u16 volatile *dst, u16 src);
void atomic_store(u8  volatile *dst, u8  src);
void atomic_store(s64 volatile *dst, s64 src);
void atomic_store(s32 volatile *dst, s32 src);
void atomic_store(s16 volatile *dst, s16 src);
void atomic_store(s8  volatile *dst, s8  src);

void atomic_exchange_add(u64 volatile *dst, u64 src);
void atomic_exchange_add(u32 volatile *dst, u32 src);
void atomic_exchange_add(u16 volatile *dst, u16 src);
void atomic_exchange_add(u8  volatile *dst, u8 src);
void atomic_exchange_add(s64 volatile *dst, s64 src);
void atomic_exchange_add(s32 volatile *dst, s32 src);
void atomic_exchange_add(s16 volatile *dst, s16 src);
void atomic_exchange_add(s8  volatile *dst, s8 src);

template<typename T>
struct Atomic {
    T value;
    
    T load() { return atomic_load(&this->value); };
    T compare_exchange(const T &desired, const T &expected) { return atomic_compare_exchange(&this->value, desired, expected); };
    b8 compare(const T &value) { return this->load() == value; };
    void store(const T &value) { atomic_store(&this->value, value); };
};
