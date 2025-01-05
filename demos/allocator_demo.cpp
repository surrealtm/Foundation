#include "memutils.h"
#include "string_type.h"
#include "os_specific.h"

#include <random>

enum Action {
    ACTION_Allocation,
    ACTION_Deallocation,
};

enum Pattern {
    PATTERN_Build_Up_And_Destroy,
    PATTERN_Random,
};

string PATTERN_NAMES[] = { "Build Up and Destroy"_s, "Random"_s };

#define MIN_ALLOCATION_SIZE 16
#define MAX_ALLOCATION_SIZE 8129
#define RANDOM_ALLOCATION_SIZE() (rand() % (MAX_ALLOCATION_SIZE - MIN_ALLOCATION_SIZE) + MIN_ALLOCATION_SIZE)

static
void do_allocations(Allocator *allocator, void **allocations, s64 count, Pattern pattern, string name) {
    Hardware_Time start = os_get_hardware_time();
    
    s64 active_allocations = 0;

    for(s64 i = 0; i < count; ++i) {
        Action action = ACTION_Allocation;
        
        switch(action) {
        case ACTION_Allocation:
            allocations[active_allocations] = allocator->allocate(RANDOM_ALLOCATION_SIZE());
            break;
        }
    }

    Hardware_Time end = os_get_hardware_time();
    printf("%.*s for %.*s took %fms.\n", (u32) PATTERN_NAMES[pattern].count, PATTERN_NAMES[pattern].data, (u32) name.count, name.data, os_convert_hardware_time(end - start, Milliseconds));

    if(allocator->_reset_procedure) {
        allocator->reset();
    } else {
        for(s64 i = 0; i < active_allocations; ++i) {
            allocator->deallocate(allocations[active_allocations]);
        }
    }
}

int main() {
    Memory_Arena underlying_arena;
    underlying_arena.create(8 * ONE_GIGABYTE);
    
    Memory_Pool underlying_pool;
    underlying_pool.create(&underlying_arena);

    Allocator arena = underlying_arena.allocator();
    Allocator heap = heap_allocator;
    Allocator pool = underlying_pool.allocator();

    const s64 count = 1000000;
    const Pattern pattern = PATTERN_Random;
    
    Resizable_Array<void *> allocations;
    allocations.reserve_exact(count);

    do_allocations(&arena, allocations.data, count, pattern, "Arena"_s);
    do_allocations(&heap, allocations.data, count, pattern, "Heap"_s);
    do_allocations(&pool, allocations.data, count, pattern, "Pool"_s);
                
    return 0;
}