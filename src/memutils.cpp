#include "memutils.h"
#include "os_specific.h"

#include <stdlib.h>



/* ------------------------------------------------ Allocator ------------------------------------------------ */

void *Allocator::allocate(u64 size) {
#if FOUNDATION_ALLOCATOR_STATISTICS
    ++this->stats.allocations;
    this->stats.working_set += size;
    if(this->stats.working_set > this->stats.peak_working_set) this->stats.peak_working_set = this->stats.working_set;

#endif

    void *pointer = this->_allocate_procedure(this->data, size);

#if FOUNDATION_ALLOCATOR_STATISTICS
    if(this->callbacks.allocation_callback)  this->callbacks.allocation_callback(this, this->callbacks.user_pointer, pointer, size);
#endif

    return pointer;
}

void Allocator::deallocate(void *pointer) {
    if(pointer == null || this->_deallocate_procedure == null) return; // Silently ignore "null" deallocations

#if FOUNDATION_ALLOCATOR_STATISTICS
    ++this->stats.deallocations;
    u64 size = this->_query_allocation_size_procedure(this->data, pointer);
    this->stats.working_set -= size;

    if(this->callbacks.deallocation_callback) this->callbacks.deallocation_callback(this, this->callbacks.user_pointer, pointer, size);
#endif

    this->_deallocate_procedure(this->data, pointer);
}

void *Allocator::reallocate(void *old_pointer, u64 new_size) {
    if(old_pointer == null) return this->allocate(new_size); // Mimick the default realloc behaviour.

#if FOUNDATION_ALLOCATOR_STATISTICS
    ++this->stats.reallocations;
    u64 old_size = this->_query_allocation_size_procedure(this->data, old_pointer);
    this->stats.working_set += (new_size - old_size);
    if(this->stats.working_set > this->stats.peak_working_set) this->stats.peak_working_set = this->stats.working_set;
#endif

    void *new_pointer = this->_reallocate_procedure(this->data, old_pointer, new_size);

#if FOUNDATION_ALLOCATOR_STATISTICS
    if(this->callbacks.reallocation_callback) this->callbacks.reallocation_callback(this, this->callbacks.user_pointer, old_pointer, old_size, new_pointer, new_size);
#endif

    return new_pointer;
}

void Allocator::reset() {
    this->_reset_procedure(this->data);
    this->reset_stats();

#if FOUNDATION_ALLOCATOR_STATISTICS
    if(this->callbacks.clear_callback) this->callbacks.clear_callback(this, this->callbacks.user_pointer);
#endif
}

void Allocator::reset_stats() {
#if FOUNDATION_ALLOCATOR_STATISTICS
    // We explicitely don't reset the peak working set here, since that may still be of interest
    // when working with scratch arenas.
    this->stats.allocations      = 0;
    this->stats.deallocations    = 0;
    this->stats.reallocations    = 0;
    this->stats.working_set      = 0;
#endif
}

u64 Allocator::query_allocation_size(void *pointer) {
    return this->_query_allocation_size_procedure(this->data, pointer);
}

#if FOUNDATION_ALLOCATOR_STATISTICS
void Allocator::print_stats(u32 indent) {
    f32 working_set_decimal, peak_working_set_decimal;
    Memory_Unit working_set_unit, peak_working_set_unit;

    working_set_unit = get_best_memory_unit(this->stats.working_set, &working_set_decimal);
    peak_working_set_unit = get_best_memory_unit(this->stats.peak_working_set, &peak_working_set_decimal);

    printf("%-*s=== Allocator ===\n", indent, "");
    printf("%-*s    Allocations:      %lld.\n", indent, "", this->stats.allocations);
    printf("%-*s    Deallocations:    %lld.\n", indent, "", this->stats.deallocations);
    printf("%-*s     -> Alive:        %lld.\n", indent, "", this->stats.allocations - this->stats.deallocations);
    printf("%-*s    Reallocations:    %lld.\n", indent, "", this->stats.reallocations);
    printf("%-*s    Working Set:      %.3f%s.\n", indent, "", working_set_decimal, memory_unit_suffix(working_set_unit));
    printf("%-*s    Peak Working Set: %.3f%s.\n", indent, "", peak_working_set_decimal, memory_unit_suffix(peak_working_set_unit));
    printf("%-*s=== Allocator ===\n", indent, "");
}
#endif



/* ---------------------------------------------- Heap Allocator ---------------------------------------------- */

void *heap_allocate(void * /*data = null */, u64 size) {
    void *pointer = null;

#if FOUNDATION_ALLOCATOR_STATISTICS
    // To support allocator statistics at runtime, we need to store the allocation
    // size somewhere. While malloc does this somewhere under the hood, we don't
    // have access to that information, so instead we need to store that size
    // ourselves. We do that by simply allocating a bigger block, and writing the
    // size at the first few bytes of the returned block. Not super elegant, but
    // better than the alternatives.
    u64 extra_size = align_to(sizeof(u64), 16, u64); // Stuff like SIMD sometimes requires 16-byte alignment...
    pointer = malloc(extra_size + size);
    if(!pointer) {
        foundation_error("A call to malloc failed for the requested size of '%lld'.", size);
        return null;
    }

    u64 *_u64 = (u64 *) pointer;
    *_u64 = size;
    pointer = (void *) ((u64) pointer + extra_size);
#else
    pointer = malloc(size);
#endif

    memset(pointer, 0, size);

    return pointer;
}

void heap_deallocate(void * /*data = null */, void *pointer) {
    if(!pointer) return;

#if FOUNDATION_ALLOCATOR_STATISTICS
    // We gave the user code an adjusted pointer, not what malloc actually returned to
    // us. Free however requires that exact pointer malloc returned, so we need to
    // readjust.
    u64 extra_size = align_to(sizeof(u64), 16, u64);
    pointer = (void *) ((u64) pointer - extra_size);
    free(pointer);
#else
    free(pointer);
#endif
}

void *heap_reallocate(void * /*data = null */, void *old_pointer, u64 new_size) {
    void *new_pointer;

#if FOUNDATION_ALLOCATOR_STATISTICS
    // We gave the user code an adjusted pointer, not what malloc actually returned to
    // us. Free however requires that exact pointer malloc returned, so we need to
    // readjust.
    u64 extra_size = align_to(sizeof(u64), 16, u64);
    old_pointer = (void *) ((u64) old_pointer - extra_size);
    new_pointer = realloc(old_pointer, new_size + extra_size);
    if(!new_pointer) {
        foundation_error("A call to malloc failed for the requested size of '%lld'.", new_size);
        return null;
    }

    u64 *_u64 = (u64 *) new_pointer;
    *_u64 = new_size;
    new_pointer = (void *) ((u64) new_pointer + extra_size);
#else
    new_pointer = realloc(old_pointer, new_size);
#endif

    return new_pointer;
}

u64 heap_query_allocation_size(void * /*data = null */, void *pointer) {
    u64 size;

#if FOUNDATION_ALLOCATOR_STATISTICS
    u64 extra_size = align_to(sizeof(u64), 16, u64);
    u64 *_u64 = (u64 *) ((u64) pointer - extra_size);
    size = *_u64;
#else
    foundation_error("FOUNDATION_ALLOCATOR_STATISTICS is off, heap_query_allocation_size is unsupported.");
    size = 0;
#endif

    return size;
}



/* ----------------------------------------------- Memory Arena ----------------------------------------------- */

void Memory_Arena::create(u64 reserved, u64 requested_commit_size, b8 executable) {
    assert(this->base == null);
    assert(reserved != 0);

    if((this->base = os_reserve_memory(reserved)) != null) {
        this->page_size   = os_get_page_size();
        this->commit_size = requested_commit_size ? align_to(requested_commit_size, this->page_size, u64) : this->page_size * 3;
        this->reserved    = align_to(reserved, this->page_size, u64);
        this->committed   = 0;
        this->size        = 0;
        this->executable  = executable;
    } else {
        this->page_size   = 0;
        this->commit_size = 0;
        this->reserved    = 0;
        this->committed   = 0;
        this->size        = 0;
        this->executable  = executable;
    }
}

void Memory_Arena::destroy() {
    assert(this->base != null); // An arena can only be destroyed once. The caller needs to ensure it has not been cleaned up yet.
    assert(this->reserved != 0);
    os_free_memory(this->base, this->reserved);
    this->base        = null;
    this->size        = 0;
    this->reserved    = 0;
    this->committed   = 0;
    this->page_size   = 0;
    this->commit_size = 0;
    this->executable  = false;
}

void Memory_Arena::reset() {
    this->release_from_mark(0);
}

u64 Memory_Arena::ensure_alignment(u64 alignment) {
    u64 padding = align_to(this->size, alignment, u64) - this->size;
    this->push(padding);
    return padding;
}

void *Memory_Arena::push(u64 size) {
    assert(this->base != null); // Make sure the arena is set up properly.

    if(this->size + size > this->committed) {
        u64 commit_size = align_to(size, this->commit_size, u64);
        assert(commit_size >= size);

        if(this->committed + commit_size <= this->reserved) {
            if(os_commit_memory((char *) this->base + this->committed, commit_size, this->executable)) {
                this->committed += commit_size;
            } else {
                foundation_error("The Memory_Arena failed to commit memory (%" PRIu64 "b requested).", commit_size);
                return null;
            }
        } else {
            foundation_error("The Memory_Arena ran out of reserved space (%" PRIu64 "b reserved, %" PRIu64 "b committed, with %" PRIu64 "b requested).", this->reserved, this->committed, size);
            return null;
        }
    }

    char *pointer = (char *) this->base + this->size;
    this->size += size;
    return pointer;
}

u64 Memory_Arena::mark() {
    return this->size;
}

void Memory_Arena::release_from_mark(u64 mark) {
    assert(mark <= this->size);

    this->size = mark;

    u64 decommit_size = ((u64) floorf((this->committed - mark) / (f32) this->commit_size)) * this->commit_size;

    if(decommit_size) {
        os_decommit_memory((char *) this->base + this->committed - decommit_size, decommit_size);
        this->committed -= decommit_size;
    }
}

void Memory_Arena::debug_print(u32 indent) {
    f32 reserved_decimal, committed_decimal, size_decimal, commit_size_decimal, os_region_decimal;
    Memory_Unit reserved_unit, committed_unit, size_unit, commit_size_unit, os_region_unit;

    reserved_unit    = get_best_memory_unit(this->reserved, &reserved_decimal);
    committed_unit   = get_best_memory_unit(this->committed, &committed_decimal);
    size_unit        = get_best_memory_unit(this->size, &size_decimal);
    commit_size_unit = get_best_memory_unit(this->commit_size, &commit_size_decimal);
    os_region_unit   = get_best_memory_unit(os_get_committed_region_size(this->base), &os_region_decimal);

    printf("%-*s=== Memory Arena ===\n", indent, "");
    printf("%-*s    Reserved:    %.3f%s.\n", indent, "", reserved_decimal, memory_unit_suffix(reserved_unit));
    printf("%-*s    Committed:   %.3f%s.\n", indent, "", committed_decimal, memory_unit_suffix(committed_unit));
    printf("%-*s    Size:        %.3f%s.\n", indent, "", size_decimal, memory_unit_suffix(size_unit));
    printf("%-*s    Commit-Size: %.3f%s.\n", indent, "", commit_size_decimal, memory_unit_suffix(commit_size_unit));
    printf("%-*s    (OS-Committed Region: %.3f%s.)\n", indent, "", os_region_decimal, memory_unit_suffix(os_region_unit));
    printf("%-*s=== Memory Arena ===\n", indent, "");
}

Allocator Memory_Arena::allocator() {
    Allocator allocator = {
        this,
        [](void *data, u64 size) -> void* { return ((Memory_Arena *) data)->push(size); },
        null,
        null,
        [](void *data) -> void { ((Memory_Arena *) data)->reset(); },
        null,
    };

    return allocator;
}



/* ----------------------------------------------- Memory Pool ----------------------------------------------- */

void *Memory_Pool::Block::data() {
    return ((char *) this) + sizeof(Memory_Pool::Block);
}

b8 Memory_Pool::Block::is_continuous_with(Block *block) {
    return (((char *) this->data()) + this->size_in_bytes) == (char *) block;
}

void Memory_Pool::Block::merge_with(Block *src) {
    if(src->next) {
        this->next = src->next;
    } else {
        this->next = null;
    }

    if(src->next_free) {
        this->next_free = src->next_free;
    } else {
        this->next_free = null;
    }

    this->size_in_bytes = this->size_in_bytes + src->size_in_bytes + sizeof(Memory_Pool::Block);
}

Memory_Pool::Block *Memory_Pool::Block::split(u64 input_size_in_bytes) {
    u64 split_size     = this->size_in_bytes - input_size_in_bytes - sizeof(Memory_Pool::Block);
    u64 split_position = align_to((u64) this->data() + input_size_in_bytes, 16, u64);
    Block *split_block = (Block *) split_position;

    if(this->next) {
        split_block->next = this->next;
    } else {
        split_block->next = null;
    }

    if(this->next_free) {
        split_block->next_free = this->next_free;
    } else {
        split_block->next_free = null;
    }

    split_block->original_allocation_size = 0;
    split_block->size_in_bytes = split_size;
    split_block->used = false;

    this->next = split_block;
    this->next_free = split_block;
    this->size_in_bytes = input_size_in_bytes;

    return split_block;
}

void Memory_Pool::create(Memory_Arena *arena) {
    this->arena           = arena;
    this->first_block     = null;
    this->last_block      = null;
    this->block_count     = 0;
    this->skip_list_count = 0;
    this->skip_list_built_for_block_count = 0;
}

void Memory_Pool::destroy() {
    this->first_block     = null;
    this->last_block      = null;
    this->block_count     = 0;
    this->skip_list_count = 0;
}

void *Memory_Pool::acquire(u64 size) {
    assert(size > 0, "Invalid requested memory pool size.");

    //
    // Query the Memory Pool for an inactive block that can be reused for this
    // allocation.
    //
    Block *unused_block = this->first_block;
    if(unused_block && unused_block->used) unused_block = unused_block->next_free;

    while(unused_block != null && unused_block->size_in_bytes < size) {
        assert(!unused_block->used, "Degenerated Memory Pool Freelist"); // Make sure we didn't degenerate somewhere
        unused_block = unused_block->next_free;
    }

    void *data = null;

    if(unused_block) {
        //
        // There exist an unused block that is big enough for this allocation to reuse it.
        //
        u64 reused_size = align_to(size, 16, u64);

        if(unused_block->size_in_bytes >= reused_size + Memory_Pool::MIN_PAYLOAD_TO_SPLIT + sizeof(Memory_Pool::Block)) {
            //
            // Split the existing block into two parts, and only use the first one.
            //
            Block *split_block = unused_block->split(reused_size);
            if(this->last_block == unused_block) this->last_block = split_block;
            ++this->block_count;
            this->maybe_rebuild_entire_skip_list();
        }

        //
        // Mark this block as used now.
        //
        unused_block->used = true;
        unused_block->original_allocation_size = size;

        //
        // All previous blocks that point to this block as being unused need to update their next_free pointer.
        //
        this->replace_free_list_entry(unused_block, unused_block->next_free);
        
        //
        // Allocators guarantee zero initialization on allocate().
        //
        data = unused_block->data();
        memset(data, 0, size);
    } else {
        //
        // The Memory Pool does not have any unused block that could be reused for
        // this allocation.
        // Allocate a new block, set it up and add it to the list.
        //

        //
        // Ensure alignment for this new block. Just increasing the arena's data size
        // would make the memory blocks non-continuous (disabling block merging on free),
        // therefore check if we are currently continuous, and then manually modify the
        // size_in_bytes of the previous block if we know that the only push on the
        // arena was us.
        //
        if(this->last_block != null && (u64) this->last_block->data() + this->last_block->size_in_bytes == (u64) this->arena->base + this->arena->size) {
            u64 padding = this->arena->ensure_alignment(16);
            this->last_block->size_in_bytes += padding;
        } else {
            this->arena->ensure_alignment(16);
        }

        Block *block = (Block *) this->arena->push(sizeof(Memory_Pool::Block) + size);

        if(block != null) { // In case the memory arena runs out of space.
            block->next          = null;
            block->next_free     = null;
            block->original_allocation_size = size;
            block->size_in_bytes = size;
            block->used          = true;

            if(this->last_block) {
                this->last_block->next = block;
                this->last_block = block;
            } else {
                this->first_block = block;
                this->last_block = block;
            }

            ++this->block_count;
            this->maybe_rebuild_entire_skip_list();
            
            data = block->data();
        }
    }

    return data;
}

void Memory_Pool::release(void *pointer) {
    if(!pointer) return;

    //
    // Find the block that corresponds to this pointer.
    //
    Block *previous_block = null;
    Block *freed_block = this->search_skip_list((Block *) ((u64) pointer - sizeof(Block)), true);

    while(freed_block && freed_block->data() < pointer) {
        previous_block = freed_block;
        freed_block = freed_block->next;
    }

    // The pointer does not correspond to a block, so this is an invalid call.
    if(!freed_block || freed_block->data() != pointer) return;

    //
    // Release this block. If possible, merge with the previous and the next block
    // so that the block list stays as small as possible.
    //
    freed_block->used = false;

    //
    // Attempt to merge with the previous block
    //
    if(previous_block && !previous_block->used && previous_block->is_continuous_with(freed_block)) {
        if(this->last_block == freed_block) this->last_block = previous_block;

        this->maybe_replace_skip_list_entry(freed_block, previous_block);
        this->replace_free_list_entry(freed_block, previous_block);
        previous_block->merge_with(freed_block);
        freed_block = previous_block;

        --this->block_count;
    }

    //
    // Attempt to merge with the next block
    //
    Block *next = freed_block->next;
    if(next && !next->used && freed_block->is_continuous_with(next)) {
        if(this->last_block == next) this->last_block = freed_block;

        this->maybe_replace_skip_list_entry(next, freed_block);
        this->replace_free_list_entry(next, freed_block);
        freed_block->merge_with(next);

        --this->block_count;
    }

    this->insert_into_free_list(freed_block);
    this->maybe_rebuild_entire_skip_list();
}

void *Memory_Pool::reacquire(void *old_pointer, u64 new_size) {
    u64 old_size = this->query_size(old_pointer);
    if(old_size == 0) return this->acquire(new_size); // The old pointer didn't actually exist, so we handle this as a reallocation

    void *new_pointer;

    if(old_size > new_size) {
        // The new allocation is smaller than the old one, meaning we can reuse the existing block, which
        // avoids a memcpy, as well as the more expensive acquire / release semantics.
        Block *reused_block = (Block *) ((char *) old_pointer - sizeof(Block));
        u64 reused_size = align_to(new_size, 16, u64);

        if(reused_block->size_in_bytes >= reused_size + Memory_Pool::MIN_PAYLOAD_TO_SPLIT + sizeof(Block)) {
            //
            // Split the existing block into two parts, and only use the first one.
            //
            Block *split_block = reused_block->split(reused_size);
            if(this->last_block == reused_block) this->last_block = split_block;
            ++this->block_count;

            this->insert_into_free_list(split_block);
            this->maybe_rebuild_entire_skip_list();
        }

        reused_block->original_allocation_size = new_size;
        new_pointer = reused_block->data();
    } else if(old_size < new_size) {
        // The new allocation is bigger than the old one, meaning we cannot reuse the existing block.
        // There is nothing we can do except a complete reallocation.
        new_pointer = this->acquire(new_size);
        memmove(new_pointer, old_pointer, min(old_size, new_size));
        this->release(old_pointer);
    } else /* old_bytes == new_bytes */ {
        new_pointer = old_pointer;
    }

    return new_pointer;
}

u64 Memory_Pool::query_size(void *pointer) {
    Block *block = this->search_skip_list((Block *) ((u64) pointer - sizeof(Block)), false);
    while(block && block->data() < pointer) {
        block = block->next;
    }

    if(block == null || block->data() != pointer) return 0;

    return block->original_allocation_size;
}

Allocator Memory_Pool::allocator() {
    Allocator allocator = {
        this,
        [](void *data, u64 size)      -> void* { return ((Memory_Pool *) data)->acquire(size); },
        [](void *data, void *pointer) -> void  { return ((Memory_Pool *) data)->release(pointer); },
        [](void *data, void *old_pointer, u64 new_size) -> void * { return ((Memory_Pool*) data)->reacquire(old_pointer, new_size); },
        [](void *data) -> void { ((Memory_Pool*) data)->destroy(); },
        [](void *data, void *pointer) -> u64   { return ((Memory_Pool *) data)->query_size(pointer); }
    };

    return allocator;
}

void Memory_Pool::maybe_rebuild_entire_skip_list() {
    const f64 rebuild_limit = 0.5;
    f64 ratio = (f64) (this->block_count - (f64) this->skip_list_built_for_block_count) / (f64) this->block_count;
    if(ratio >= -rebuild_limit && ratio <= rebuild_limit) return;

    this->skip_list_count = 0;
    this->skip_list_built_for_block_count = this->block_count;

    if(this->block_count < Memory_Pool::SKIP_LIST_CAPACITY) {
        // Just insert all blocks into the skip list
        Block *block = this->first_block;
        while(block) {
            this->skip_list[this->skip_list_count] = block;
            ++this->skip_list_count;
            block = block->next;
        }
    } else {
        // Insert roughly any n'th block into the skip list
        f64 n = (f64) this->block_count / (f64) Memory_Pool::SKIP_LIST_CAPACITY;
        f64 accum = n; // We want the first pool to be the first entry of the skiplist, as updating the next_free pointers in the acquire procedure depends on that.

        Block *block = this->first_block;
        while(block && this->skip_list_count < Memory_Pool::SKIP_LIST_CAPACITY) {
            if(accum >= n) {
                this->skip_list[this->skip_list_count] = block;
                ++this->skip_list_count;
                accum -= n;
            }

            accum += 1;
            block = block->next;
        }
    }
}

s64 Memory_Pool::search_skip_list_for_index(Memory_Pool::Block *block_pointer, b8 return_previous) {
    if(this->skip_list_count == 0) return -1;

    s64 low = 0;
    s64 high = this->skip_list_count - 1;

    while(low < high) {
        s64 median = (low + high) / 2;
        if(block_pointer == this->skip_list[median]) {
            return median;
        } else if(block_pointer > this->skip_list[median]) {
            low = median + 1;
        } else {
            high = median - 1;
        }
    }

    if((return_previous && low > 0 && skip_list[low] == block_pointer) || (this->skip_list[low] > block_pointer)) --low;

    return low;
}

Memory_Pool::Block *Memory_Pool::search_skip_list(Memory_Pool::Block *block_pointer, b8 return_previous) {
    if(this->skip_list_count == 0) return this->first_block;
    s64 index = this->search_skip_list_for_index(block_pointer, return_previous);
    return this->skip_list[index];
}

void Memory_Pool::maybe_replace_skip_list_entry(Memory_Pool::Block *to_replace, Memory_Pool::Block *replace_with) {
    s64 i = this->search_skip_list_for_index(to_replace, false);
    if(i == -1 || this->skip_list[i] != to_replace) return;

    if(this->search_skip_list_for_index(replace_with, false) != -1) {
        // Avoid duplicates in the skip list
        if(i + 1 < this->skip_list_count) memcpy(&this->skip_list[i], &this->skip_list[i + 1], (this->skip_list_count - i - 1) * sizeof(Block));
        --this->skip_list_count;
    } else {
        this->skip_list[i] = replace_with;
    }
}

void Memory_Pool::insert_into_free_list(Memory_Pool::Block *freed_block) {
    b8 found_previous_free_block = false;
    s64 index = this->search_skip_list_for_index(freed_block, false);
    Block *end_of_range = freed_block;

    while(!found_previous_free_block && index >= 0) {
        Block *block = this->skip_list[index];

        while(block != end_of_range) {
            if(block->next_free != null && block->next_free < freed_block) found_previous_free_block = true;
            if(block->next_free >= freed_block) block->next_free = freed_block;
            block = block->next;
        }

        end_of_range = this->skip_list[index];
        --index;
    }
}

void Memory_Pool::replace_free_list_entry(Memory_Pool::Block *to_replace, Memory_Pool::Block *replace_with) {
    b8 found_previous_free_block = false;
    s64 index = this->search_skip_list_for_index(to_replace, false);
    Block *end_of_range = to_replace;

    while(!found_previous_free_block && index >= 0) {
        Block *block = this->skip_list[index];

        while(block != end_of_range) {
            if(block->next_free < to_replace)  found_previous_free_block = true;
            if(block->next_free == to_replace) block->next_free = replace_with;
            block = block->next;
        }

        end_of_range = this->skip_list[index];
        --index;
    }    
}




/* -------------------------------------------- Builtin Allocators -------------------------------------------- */

Allocator heap_allocator = { null, heap_allocate, heap_deallocate, heap_reallocate, null, heap_query_allocation_size };
Allocator *Default_Allocator = &heap_allocator;

thread_local Memory_Arena temp_arena;
thread_local Allocator temp;

void create_temp_allocator(u64 reserved) {
    if(temp_arena.base) return; // Already initialized in this thread.

    temp_arena.create(reserved);
    temp = temp_arena.allocator();
}

void destroy_temp_allocator() {
    temp.reset();
}

u64 mark_temp_allocator() {
    return temp_arena.mark();
}

void release_temp_allocator(u64 mark) {
    temp_arena.release_from_mark(mark);

#if FOUNDATION_ALLOCATOR_STATISTICS
    temp.stats.working_set = mark;
#endif
}



/* -------------------------------------------------- Utils -------------------------------------------------- */

const char *memory_unit_suffix(Memory_Unit unit) {
    const char *string;

    switch(unit) {
    case Bytes:      string = "b"; break;
    case Kilobytes:  string = "kb"; break;
    case Megabytes:  string = "mb"; break;
    case Gigabytes:  string = "gb"; break;
    case Terrabytes: string = "tb"; break;
    default: string = "<>"; break;
    }

    return string;
}

Memory_Unit get_best_memory_unit(s64 bytes, f32 *decimal) {
    Memory_Unit unit = Bytes;
    *decimal = (f32) bytes;

    while(unit < MEMORY_UNIT_COUNT && bytes >= 1000) {
        unit = (Memory_Unit) (unit + 1);
        *decimal = (f32) bytes / 1000.0f;
        bytes /= 1000;
    }

    return unit;
}

f64 convert_to_memory_unit(s64 bytes, Memory_Unit target_unit) {
    f64 decimal;

    switch(target_unit) {
    case Bytes:      decimal = (f64) bytes; break;
    case Kilobytes:  decimal = (f64) bytes / 1000.0; break;
    case Megabytes:  decimal = (f64) bytes / 1000000.0; break;
    case Gigabytes:  decimal = (f64) bytes / 1000000000.0; break;
    case Terrabytes: decimal = (f64) bytes / 1000000000000.0; break;
    default: decimal = 1; break;
    }

    return decimal;
}


#if FOUNDATION_ALLOCATOR_STATISTICS
static
void __allocation_callback(Allocator *allocator, const void *allocator_name, void *data, u64 size) {
    printf("[Allocation] %s : %" PRIu64 "b, 0x%016" PRIx64 "\n", (char *) allocator_name, size, (u64) data);
}

static
void __deallocation_callback(Allocator *allocator, const void *allocator_name, void *data, u64 size) {
    printf("[Deallocation] %s : %" PRIu64 "b, 0x%016" PRIx64 "\n", (char *) allocator_name, size, (u64) data);
}

static
void __reallocation_callback(Allocator *allocator, const void *allocator_name, void *old_data, u64 old_size, void *new_data, u64 new_size) {
    printf("[Reallocation] %s : %" PRIu64 "b, 0x%016" PRIx64 " -> %" PRIu64 "b, 0x%016" PRIx64 "\n", (char *) allocator_name, old_size, (u64) old_data, new_size, (u64) new_data);
}

static
void __clear_callback(Allocator *allocator, const void *allocator_name) {
    printf("[Clear] %s\n", (char *) allocator_name);
}

void install_allocator_console_logger(Allocator *allocator, const char *name) {
    allocator->callbacks.allocation_callback   = __allocation_callback;
    allocator->callbacks.deallocation_callback = __deallocation_callback;
    allocator->callbacks.reallocation_callback = __reallocation_callback;
    allocator->callbacks.clear_callback        = __clear_callback;
    allocator->callbacks.user_pointer          = name;
}

void clear_allocator_logger(Allocator *allocator) {
    allocator->callbacks.allocation_callback   = null;
    allocator->callbacks.deallocation_callback = null;
    allocator->callbacks.reallocation_callback = null;
    allocator->callbacks.clear_callback        = null;
    allocator->callbacks.user_pointer          = null;
}
#endif



void byteswap2(void *value) {
    u8 *bytes = (u8 *) value;
    *(u16 *) value = (bytes[0] << 8) | (bytes[1]);
}

void byteswap4(void *value) {
    u8 *bytes = (u8 *) value;
    *(u32 *) value = (bytes[0] << 24) | (bytes[1] << 16) | (bytes[2] << 8) | (bytes[3]);
}

void byteswap8(void *value) {
    u8 *bytes = (u8 *) value;
    *(u64 *) value =  ((u64) bytes[0] << 56ULL) | ((u64) bytes[1] << 48ULL) | ((u64) bytes[2] << 40ULL) | ((u64) bytes[3] << 32ULL) | ((u64) bytes[4] << 24ULL) | ((u64) bytes[5] << 16ULL) | ((u64) bytes[6] << 8ULL) | ((u64) bytes[7]);
}
