#include "memutils.h"
#include "string_type.h"
#include "os_specific.h"

#include <random>

struct Ela {
    // nocheckin: The problem is that when merging blocks together, they may have to go into a seperate bin, which
    // we don't do right now...
    static const s64 BIN_COUNT = 1;

    struct Block {
        static const s64 HEADER_SIZE = 16; // Ought to be 16-byte aligned...

        // These are appended ahead of the payload
        u64 block_size_in_bytes; // The actual size in bytes of the block
        u64 user_size_in_bytes;  // The user requested (unaligned, unmerged) size in bytes  // nocheckin

        // These live inside the payload when the block is freed. Since a
        // payload is always aligned to 16 bytes, we can assume the payload
        // is always at least 16 bytes large, allowing us to store these two
        // pointers.
        Block *next_free;
        Block *previous_free;
    };

    struct Bin {
        Block *first;
    };

    static_assert(sizeof(Block) % 16 == 0, "A block must be 16 byte aligned.");

    Memory_Arena *arena;
    Bin bins[BIN_COUNT];

    void create(Memory_Arena *arena) {
        this->arena = arena;
        memset(this->bins, 0, sizeof(this->bins));
    }

    void *acquire(u64 user_size_in_bytes) {
        u64 aligned_size_in_bytes = (user_size_in_bytes + 0xf) & (~0xf); // Align to 16 bytes.
        assert(aligned_size_in_bytes >= 16);

        //
        // Try to find a block in the free-list that can accomodate this allocation
        //
        Block **pointer_to_free_block = this->get_free_list_for_size(aligned_size_in_bytes);
        
        while(*pointer_to_free_block) {
            Block *free_block = *pointer_to_free_block;

            if(free_block->block_size_in_bytes >= aligned_size_in_bytes && free_block->block_size_in_bytes <= aligned_size_in_bytes + Block::HEADER_SIZE) {
                // This existing free block is large enough to hold the new user payload, but small enough
                // that splitting doesn't make sense. Therefore, just reuse the entire block.
                free_block->user_size_in_bytes = user_size_in_bytes;
                if(free_block->next_free) free_block->next_free->previous_free = free_block->previous_free;
                *pointer_to_free_block = free_block->next_free;

                return (void *) ((u64) free_block + Block::HEADER_SIZE);
            } else if(free_block->block_size_in_bytes > aligned_size_in_bytes) {
                // The existing block is so large that splitting it makes sense. Replace the free_block
                // with a different block, which is located inside free_block's payload. We need to insert
                // the split block into the free list here, as it may very well by in a different bin.
                if(free_block->previous_free) free_block->previous_free->next_free = free_block->next_free;
                if(free_block->next_free)     free_block->next_free->previous_free = free_block->previous_free;
                *pointer_to_free_block = free_block->next_free;

                Block *split_block = (Block *) ((u64) free_block + Block::HEADER_SIZE + aligned_size_in_bytes);
                split_block->block_size_in_bytes = free_block->block_size_in_bytes - Block::HEADER_SIZE - aligned_size_in_bytes;
                split_block->user_size_in_bytes  = 0;
                this->insert_new_block_into_free_list(split_block);

                free_block->block_size_in_bytes = aligned_size_in_bytes;
                free_block->user_size_in_bytes  = user_size_in_bytes;
                
                return (void *) ((u64) free_block + Block::HEADER_SIZE);
            } else {
                pointer_to_free_block = &(*pointer_to_free_block)->next_free;
            }
        }

        //
        // Allocate a new block and return the user pointer
        //
        Block *block = (Block *) this->arena->push(Block::HEADER_SIZE + aligned_size_in_bytes);
        block->block_size_in_bytes = aligned_size_in_bytes;
        block->user_size_in_bytes  = user_size_in_bytes;
        return (void *) ((u64) block + Block::HEADER_SIZE);
    }

    void *reacquire(void *old_pointer, u64 user_size_in_bytes) {
        u64 aligned_size_in_bytes = (user_size_in_bytes + 0xf) & (~0xf); // Align to 16 bytes.
        assert(aligned_size_in_bytes >= 16);

        Block *block = (Block *) ((u64) old_pointer - Block::HEADER_SIZE);
        
        if(block->block_size_in_bytes >= aligned_size_in_bytes && block->block_size_in_bytes <= aligned_size_in_bytes + Block::HEADER_SIZE) {
            // The size didn't change significantly, so don't change anything about the underlying
            // block structure...
            block->user_size_in_bytes = user_size_in_bytes;
            return (void *) ((u64) block + Block::HEADER_SIZE);
        } else if(block->block_size_in_bytes > aligned_size_in_bytes) {
            // The new size is significantly smaller than the previous one, so split the existing
            // block up. This avoids having to copy the existing data
            Block *split_block = (Block *) ((u64) block + Block::HEADER_SIZE + aligned_size_in_bytes);
            split_block->block_size_in_bytes = block->block_size_in_bytes - Block::HEADER_SIZE - aligned_size_in_bytes;
            split_block->user_size_in_bytes  = 0;
            this->insert_new_block_into_free_list(split_block);

            block->block_size_in_bytes = aligned_size_in_bytes;
            block->user_size_in_bytes  = user_size_in_bytes;
            
            return (void *) ((u64) block + Block::HEADER_SIZE);
        } else if((u64) block + Block::HEADER_SIZE + block->block_size_in_bytes == (u64) this->arena->base + this->arena->size) {
            // The new size is larger than the previous, but this block is the very last thing in
            // the arena. In that case, we can just push the arena to get more space without
            // having to do copy the existing data.
            u64 aligned_addition = aligned_size_in_bytes - block->block_size_in_bytes;
            this->arena->push(aligned_addition);
            block->block_size_in_bytes += aligned_addition;
            block->user_size_in_bytes   = user_size_in_bytes;
            return (void *) ((u64) block + Block::HEADER_SIZE);
        } else {
            // The new size is larger than the previous, and we cannot abuse the layout of the
            // pool in the arena, so we just have to allocate a new block, and then free the
            // old one...
            void *new_pointer = this->acquire(user_size_in_bytes);
            memcpy(new_pointer, old_pointer, block->user_size_in_bytes);
            this->release(old_pointer);
            return new_pointer;
        }
    }

    void release(void *pointer) {
        Block *free_block = (Block *) ((u64) pointer - Block::HEADER_SIZE);
        free_block->user_size_in_bytes = 0;
        this->insert_new_block_into_free_list(free_block);
    }

    void destroy() {
        memset(this->bins, 0, sizeof(this->bins));
    }
    
    u64 query_size(void *pointer) {
        Block *block = (Block *) ((u64) pointer - Block::HEADER_SIZE);
        return block->user_size_in_bytes;
    }

    void maybe_merge_blocks(Block *previous, Block *next) {
        // Merge with the next block in the list
        if((Block *) ((u64) previous + Block::HEADER_SIZE + previous->block_size_in_bytes) == next) {
            previous->block_size_in_bytes += Block::HEADER_SIZE + next->block_size_in_bytes;
            previous->next_free = next->next_free;
            if(previous->next_free) previous->next_free->previous_free = previous;
        }
    }

    void maybe_merge_block_with_neighbors(Block *block) {
        if(block->next_free) maybe_merge_blocks(block, block->next_free);
        if(block->previous_free) maybe_merge_blocks(block->previous_free, block);
    }
    
    void insert_new_block_into_free_list(Block *free_block) {
        Block **free_list = this->get_free_list_for_size(free_block->block_size_in_bytes);
        
        // Insert into the free list here at a sorted position, so that we can always guarantee merges...
        if(*free_list == null) {
            free_block->next_free = null;
            free_block->previous_free = null;
            *free_list = free_block;
        } else if(free_block < *free_list) {
            free_block->next_free = *free_list;
            free_block->next_free->previous_free = free_block;
            free_block->previous_free = null;
            *free_list = free_block;
            this->maybe_merge_block_with_neighbors(free_block);
        } else {
            Block *previous = *free_list;
            while(previous->next_free != null && previous->next_free < free_block) {
                previous = previous->next_free;
            }
            
            free_block->next_free = previous->next_free;
            if(free_block->next_free) free_block->next_free->previous_free = free_block;
            free_block->previous_free = previous;
            previous->next_free = free_block;
            this->maybe_merge_block_with_neighbors(free_block);
        }        
    }

    Block **get_free_list_for_size(u64 aligned_size_in_bytes) {
        u64 transformed_size_in_bytes = min(aligned_size_in_bytes >> 4, (1 << BIN_COUNT) - 1); // The size is aligned to 16 bytes, so we cannot get smaller sizes than that... Therefore the smallest bin should be for allocations of size 16.
        u64 index = os_highest_bit_set(transformed_size_in_bytes);
        return &this->bins[index].first;
    }

    void debug_print_free_lists() {
        for(s64 i = 0; i < BIN_COUNT; ++i) {
            printf("Bin %" PRId64 ":\n  ", i);
            
            Block *block = this->bins[i].first;
            while(block) {
                printf(" %" PRIu64 " | ", block->block_size_in_bytes);
                block = block->next_free;
            }

            printf("\n");
        }
    }

    Allocator allocator() {
        Allocator allocator = {
            this,
            [](void *data, u64 size)      -> void* { return ((Ela *) data)->acquire(size); },
            [](void *data, void *pointer) -> void  { return ((Ela *) data)->release(pointer); },
            [](void *data, void *old_pointer, u64 new_size) -> void * { return ((Ela *) data)->reacquire(old_pointer, new_size); },
            [](void *data) -> void { ((Ela *) data)->destroy(); },
            [](void *data, void *pointer) -> u64   { return ((Ela *) data)->query_size(pointer); }
        };

        return allocator;
    }
};



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
            allocations[index] = allocator->reallocate(allocations[index], RANDOM_ALLOCATION_SIZE());
            *(s64 *) allocations[index] = index; // Touch the allocation to cause a potential page fault, to simulate "real" usage of the allocations... Otherwise the Memory Arena just goes brrrrr
        } break;
        }
    }

    Hardware_Time end = os_get_hardware_time();
    printf("%.*s for %.*s took %fms.\n", (u32) PATTERN_NAMES[pattern].count, PATTERN_NAMES[pattern].data, (u32) name.count, name.data, os_convert_hardware_time(end - start, Milliseconds));

    printf(" >> Alive allocations: %" PRId64 ", Peak working set: %" PRId64 "\n", active_allocations, allocator->stats.peak_working_set);

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

    Ela underlying_ela;
    underlying_ela.create(&underlying_arena);

    Allocator arena = underlying_arena.allocator();
    Allocator heap = heap_allocator;
    Allocator pool = underlying_pool.allocator();
    Allocator ela = underlying_ela.allocator();

    const Pattern pattern = PATTERN_Random;
    const s64 count = 100000;
    
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
    debug_print_arena(underlying_ela.arena, "ELA"_s);
    underlying_ela.debug_print_free_lists();

    return 0;
}