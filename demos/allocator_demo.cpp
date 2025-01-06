#include "memutils.h"
#include "string_type.h"
#include "os_specific.h"
#include "new_memory_pool.h"

#include <random>

enum Action {
    ACTION_Allocation,
    ACTION_Deallocation,
    ACTION_Reallocation,
};

enum Pattern {
    PATTERN_Only_Allocations,
    PATTERN_Build_Up_And_Destroy,
    PATTERN_Random,
};

string PATTERN_NAMES[] = { "Only Allocations"_s, "Build Up and Destroy"_s, "Random"_s };

#define MIN_ALLOCATION_SIZE 16
#define MAX_ALLOCATION_SIZE 8129
#define RANDOM_ALLOCATION_SIZE() (rand() % (MAX_ALLOCATION_SIZE - MIN_ALLOCATION_SIZE) + MIN_ALLOCATION_SIZE)

static
void do_allocations(Allocator *allocator, void **allocations, s64 count, Pattern pattern, string name) {
    srand(548375543);
    
    Hardware_Time start = os_get_hardware_time();
    
    s64 active_allocations = 0;

    for(s64 i = 0; i < count; ++i) {
        Action action;

        switch(pattern) {
        case PATTERN_Only_Allocations:
            action = ACTION_Allocation;
            break;

        case PATTERN_Build_Up_And_Destroy:
            action = (i * 2 <= count) ? ACTION_Allocation : ACTION_Deallocation;
            break;

        case PATTERN_Random: {
            int index = rand() % 3;
            switch(index) {
            case 0: action = ACTION_Allocation; break;
            case 1: action = ACTION_Deallocation; break;
            case 2: action = ACTION_Reallocation; break;
            }    
        } break;
        }
        
        switch(action) {
        case ACTION_Allocation:
            allocations[active_allocations] = allocator->allocate(RANDOM_ALLOCATION_SIZE());
            *(s64 *) allocations[active_allocations] = active_allocations; // Touch the allocation to cause a potential page fault, to simulate "real" usage of the allocations... Otherwise the Memory Arena just goes brrrrr
            ++active_allocations;
            break;

        case ACTION_Deallocation: {
            if(active_allocations == 0) break;
            s64 index = rand() % active_allocations;
            allocator->deallocate(allocations[index]);
            memmove(&allocations[index], &allocations[index + 1], (active_allocations - index - 1) * sizeof(void *));
            --active_allocations;
        } break;

        case ACTION_Reallocation: {
            if(active_allocations == 0) break;
            s64 index = rand() % active_allocations;
            void *new_pointer = allocator->reallocate(allocations[index], RANDOM_ALLOCATION_SIZE());
            allocations[index] = new_pointer;
            *(s64 *) allocations[index] = index; // Touch the allocation to cause a potential page fault, to simulate "real" usage of the allocations... Otherwise the Memory Arena just goes brrrrr
        } break;
        }
    }

    Hardware_Time end = os_get_hardware_time();
    printf("%.*s for %.*s took %fms.\n", (u32) PATTERN_NAMES[pattern].count, PATTERN_NAMES[pattern].data, (u32) name.count, name.data, os_convert_hardware_time(end - start, Milliseconds));

#if FOUNDATION_ALLOCATOR_STATISTICS
    printf(" >> Alive allocations: %" PRId64 ", Peak working set: %" PRId64 "\n", active_allocations, allocator->stats.peak_working_set);
#endif

    if(allocator->_reset_procedure) {
        allocator->reset();
    } else {
        for(s64 i = 0; i < active_allocations; ++i) {
            allocator->deallocate(allocations[i]);
        }
    }
}

static
void debug_print_arena(Memory_Arena *arena, string name) {
    f64 megabytes = convert_to_memory_unit(arena->committed, Megabytes);
    printf(" >> %.*s: Committed %fmb\n", (u32) name.count, name.data, megabytes);
}

int main() {
    Memory_Arena underlying_arena;
    underlying_arena.create(8 * ONE_GIGABYTE, 128 * ONE_KILOBYTE);
    
    Memory_Pool underlying_pool;
    underlying_pool.create(&underlying_arena);

    Ela::Memory_Pool underlying_ela;
    underlying_ela.create(8 * ONE_GIGABYTE, 128 * ONE_KILOBYTE);

    Allocator arena = underlying_arena.allocator();
    Allocator heap = heap_allocator;
    Allocator pool = underlying_pool.allocator();
    Allocator ela = underlying_ela.allocator();

    const Pattern pattern = PATTERN_Random;
    const s64 count = 1000000;
    
    Resizable_Array<void *> allocations;
    allocations.reserve_exact(count);

    /*
    do_allocations(&arena, allocations.data, count, pattern, "Arena"_s);
    debug_print_arena(&arena, "Arena"_s);
    underlying_arena.reset();
    */

    /*
    do_allocations(&pool, allocations.data, count, pattern, "Pool"_s);
    debug_print_arena(underlying_pool.arena, "Pool"_s);
    underlying_arena.reset();
    */
 
    do_allocations(&heap, allocations.data,  count, pattern, "Heap"_s);

    do_allocations(&ela, allocations.data,   count, pattern, "Ela"_s);

    return 0;
}