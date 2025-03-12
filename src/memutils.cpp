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

b8 Memory_Pool::block_boundaries_look_valid(Memory_Pool::Block_Header *header) {
    Block_Footer *footer = this->get_footer_from_header(header);
    return footer->block_size_in_bytes == header->block_size_in_bytes && header->status == footer->status;
}

b8 Memory_Pool::blocks_are_continuous(Memory_Pool::Block_Header *prev, Memory_Pool::Block_Header *next) {
    return (u64) prev + prev->block_size_in_bytes + Memory_Pool::METADATA_SIZE == (u64) next;
}

void *Memory_Pool::get_user_pointer_from_header(Memory_Pool::Block_Header *header) {
    return (void *) ((u64) header + Memory_Pool::HEADER_SIZE);    
}

Memory_Pool::Block_Footer *Memory_Pool::get_footer_from_header(Memory_Pool::Block_Header *header) {
    return (Block_Footer *) ((u64) header + Memory_Pool::HEADER_SIZE + header->block_size_in_bytes);
}

Memory_Pool::Block_Header *Memory_Pool::get_header_from_footer(Memory_Pool::Block_Footer *footer) {
    return (Block_Header *) ((u64) footer - Memory_Pool::HEADER_SIZE - footer->block_size_in_bytes);
}

Memory_Pool::Block_Header *Memory_Pool::get_header_from_user_pointer(void *user_pointer) {
    return (Block_Header *) ((u64) user_pointer - HEADER_SIZE);
}

Memory_Pool::Block_Footer *Memory_Pool::get_footer_from_user_pointer(void *user_pointer) {
    Block_Header *header = this->get_header_from_user_pointer(user_pointer);
    return this->get_footer_from_header(header);
}

Memory_Pool::Block_Header *Memory_Pool::get_next_header_from_header(Memory_Pool::Block_Header *header) {
    // Make sure we're not actually the last block in the arena...
    Block_Header *next_header = (Block_Header *) ((u64) header + METADATA_SIZE + header->block_size_in_bytes);
    return ((u64) next_header < (u64) this->arena.base + this->arena.size) ? next_header : null;
}

Memory_Pool::Block_Header *Memory_Pool::get_previous_header_from_header(Memory_Pool::Block_Header *header) {
    // Make sure we're not actually the last block in the arena...
    Block_Footer *previous_footer = (Block_Footer *) ((u64) header - FOOTER_SIZE);
    Block_Header *previous_header = this->get_header_from_footer(previous_footer);
    return ((u64) previous_header >= (u64) this->arena.base + 8) ? previous_header : null;
}

Memory_Pool::Block_Header *Memory_Pool::split_block(Memory_Pool::Block_Header *existing, u64 existing_block_size_in_bytes, u64 existing_user_size_in_bytes) {
    Block_Header *split = (Block_Header *) ((u64) existing + METADATA_SIZE + existing_block_size_in_bytes);
    this->update_block_size_in_bytes(split, existing->block_size_in_bytes - existing_block_size_in_bytes - METADATA_SIZE, 0);
    this->update_block_size_in_bytes(existing, existing_block_size_in_bytes, existing_user_size_in_bytes);
    return split;
}

void Memory_Pool::update_block_size_in_bytes(Memory_Pool::Block_Header *header, u64 block_size_in_bytes, u64 user_size_in_bytes) {
    assert(block_size_in_bytes < MAX_S32);
    header->block_size_in_bytes = (u32) block_size_in_bytes;
    header->user_size_in_bytes  = (u32) user_size_in_bytes;
    Block_Footer *footer  = this->get_footer_from_header(header);
    footer->block_size_in_bytes = (u32) block_size_in_bytes;
    footer->user_size_in_bytes  = (u32) user_size_in_bytes;
}

void Memory_Pool::update_block_status(Memory_Pool::Block_Header *header, u64 status) {
    header->status = status;
    Block_Footer *footer = this->get_footer_from_header(header);
    footer->status = status;
}


s64 Memory_Pool::get_bin_index_for_size(u64 aligned_size_in_bytes) {
    // The size is aligned to 16 bytes, so we cannot get smaller sizes than that... Therefore
    // the smallest bin should be for allocations of size 16.
    u64 transformed_size_in_bytes = min(aligned_size_in_bytes >> 4, (1 << BIN_COUNT) - 1);
    return os_highest_bit_set(transformed_size_in_bytes);
}

void Memory_Pool::insert_block_into_free_list(Memory_Pool::Block_Header *free_block) {
    assert(this->block_boundaries_look_valid(free_block) && free_block->status == BLOCK_FREE);
    Block_Header **free_list  = &this->bins[this->get_bin_index_for_size(free_block->block_size_in_bytes)].first;
    free_block->next_free     = *free_list;
    if(free_block->next_free) free_block->next_free->previous_free = free_block;
    free_block->previous_free = null;
    *free_list = free_block;
}

void Memory_Pool::remove_block_from_free_list(Memory_Pool::Block_Header *free_block) {
    Block_Header **free_list  = &this->bins[this->get_bin_index_for_size(free_block->block_size_in_bytes)].first;
    if(free_block->previous_free) free_block->previous_free->next_free = free_block->next_free;
    if(free_block->next_free)     free_block->next_free->previous_free = free_block->previous_free;
    if(*free_list == free_block) *free_list = free_block->next_free;
}

Memory_Pool::Block_Header *Memory_Pool::maybe_coalesce_free_block(Memory_Pool::Block_Header *free_block) {
    //
    // Find adjacent blocks that are free and can be coalesced with this new free block.
    // If they can be merged together, remove the existing blocks from the free list, as the size
    // has changed and they may therefore need to be moved into a different bin. This new freed
    // block hasn't yet been added to the free list, so it does not have to be removed, but
    // it also doesn't have valid free-list pointers yet.
    // 

    Block_Header *returned = free_block;
    
    Block_Header *next = this->get_next_header_from_header(free_block);

    if(next && next->status == BLOCK_FREE && this->blocks_are_continuous(free_block, next)) {
        this->remove_block_from_free_list(next);
        this->update_block_size_in_bytes(free_block, free_block->block_size_in_bytes + next->block_size_in_bytes + METADATA_SIZE, 0);
        this->update_block_status(free_block, BLOCK_FREE); // Since the size changed, the position of the footer changed...
        assert(this->block_boundaries_look_valid(free_block) && free_block->status == BLOCK_FREE);
    }
    
    Block_Header *previous = this->get_previous_header_from_header(free_block);

    if(previous && previous->status == BLOCK_FREE && this->blocks_are_continuous(previous, free_block)) {
        this->remove_block_from_free_list(previous);
        this->update_block_size_in_bytes(previous, previous->block_size_in_bytes + free_block->block_size_in_bytes + METADATA_SIZE, 0);
        this->update_block_status(previous, BLOCK_FREE); // Since the size changed, the position of the footer changed...
        assert(this->block_boundaries_look_valid(previous) && previous->status == BLOCK_FREE);
        returned = previous;
    }
    
    return returned;
}


void Memory_Pool::create(u64 reserved, u64 requested_commit_size) {
    this->arena.create(reserved, requested_commit_size);
    this->arena.push(8); // We want user-payloads to be 16-byte aligned. The start of the arena is 16-byte aligned, but every header we push adds 8 bytes. This makes sure that the payload is always 16-byte aligned (as after the header comes the payload, and after that comes 8 bytes of footer)
    memset(this->bins, 0, sizeof(this->bins));
}

void Memory_Pool::destroy() {
    this->arena.destroy();
    memset(this->bins, 0, sizeof(this->bins));
}

void Memory_Pool::reset() {
    this->arena.reset();
    memset(this->bins, 0, sizeof(this->bins));
}

void *Memory_Pool::allocate(u64 user_size_in_bytes) {
    u64 aligned_size_in_bytes = (user_size_in_bytes + 0xf) & (~0xf); // Align to 16 bytes.

    //
    // Try to find a block in the free-list that can accomodate this allocation
    //
    for(s64 bin = this->get_bin_index_for_size(aligned_size_in_bytes); bin < BIN_COUNT; ++bin) {
        Block_Header *free_block = this->bins[bin].first;
        
        while(free_block) {
            assert(free_block->status == BLOCK_FREE);

            if(free_block->block_size_in_bytes >= aligned_size_in_bytes && free_block->block_size_in_bytes <= aligned_size_in_bytes + METADATA_SIZE) {
                // This existing free block is large enough to hold the new user payload, but small
                // enough that splitting doesn't make sense. Therefore, just reuse the entire block.
                this->update_block_size_in_bytes(free_block, free_block->block_size_in_bytes, user_size_in_bytes);
                this->update_block_status(free_block, BLOCK_IN_USE);
                this->remove_block_from_free_list(free_block);
                assert(this->block_boundaries_look_valid(free_block));
                return this->get_user_pointer_from_header(free_block);
            } else if(free_block->block_size_in_bytes >= aligned_size_in_bytes) {
                // The existing block is so large that splitting it makes sense. Split the block,
                // removing this one from and add the split part to the freelist.
                this->remove_block_from_free_list(free_block);
                
                Block_Header *split_block = this->split_block(free_block, aligned_size_in_bytes, user_size_in_bytes);
                this->update_block_status(split_block, BLOCK_FREE);
                this->insert_block_into_free_list(split_block);
                assert(this->block_boundaries_look_valid(split_block));

                this->update_block_status(free_block, BLOCK_IN_USE);
                assert(this->block_boundaries_look_valid(free_block));
                return this->get_user_pointer_from_header(free_block);
            } else {
                // This block is too small, try the next one in this bin.
                free_block = free_block->next_free;
            }
        }
    }
    
    //
    // Allocate a new block and return the user pointer.
    //
    Block_Header *header = (Block_Header * ) this->arena.push(aligned_size_in_bytes + METADATA_SIZE);
    this->update_block_size_in_bytes(header, aligned_size_in_bytes, user_size_in_bytes);
    this->update_block_status(header, BLOCK_IN_USE);
    assert(this->block_boundaries_look_valid(header));
    return this->get_user_pointer_from_header(header);
}

void *Memory_Pool::reallocate(void *old_pointer, u64 new_user_size_in_bytes) {
    u64 new_aligned_size_in_bytes = (new_user_size_in_bytes + 0xf) & (~0xf); // Align to 16 bytes.

    Block_Header *old_block = this->get_header_from_user_pointer(old_pointer);
    assert(old_block->status == BLOCK_IN_USE && this->block_boundaries_look_valid(old_block));
    
    if(old_block->block_size_in_bytes >= new_aligned_size_in_bytes - METADATA_SIZE && old_block->block_size_in_bytes <= new_aligned_size_in_bytes + METADATA_SIZE) {
        // The size didn't change significantly, so don't change anything about the underlying
        // block structure...
        this->update_block_size_in_bytes(old_block, old_block->block_size_in_bytes, new_user_size_in_bytes);
        return old_pointer;
    } else if(old_block->block_size_in_bytes >= new_aligned_size_in_bytes) {
        // The new size is smaller than the previous one, so split the existing block up.
        // This avoids having to copy the existing data.
        Block_Header *split_block = this->split_block(old_block, new_aligned_size_in_bytes, new_user_size_in_bytes);
        this->update_block_status(split_block, BLOCK_FREE);
        this->insert_block_into_free_list(split_block);
        assert(this->block_boundaries_look_valid(split_block));
    
        this->update_block_status(old_block, BLOCK_IN_USE); // Since the size changed, the position of the footer changed...
        assert(this->block_boundaries_look_valid(old_block));
        return this->get_user_pointer_from_header(old_block);
    } else if((u64) this->get_footer_from_header(old_block) == (u64) this->arena.base + this->arena.size - FOOTER_SIZE) {
        // The new size is larger than the previous, but this block is the very last thing in the
        // arena. In that case, we can just push the arena to get more space without having to
        // copy the existing data.
        u64 aligned_addition = new_aligned_size_in_bytes - old_block->block_size_in_bytes;
        this->arena.push(aligned_addition);
        this->update_block_size_in_bytes(old_block, old_block->block_size_in_bytes + aligned_addition, new_user_size_in_bytes);
        this->update_block_status(old_block, BLOCK_IN_USE); // Since the size changed, the position of the footer changed...
        assert(this->block_boundaries_look_valid(old_block));
        return this->get_user_pointer_from_header(old_block);
    } else {
        // The new size is larger than the previous, and we cannot abuse the layout of the pool
        // in the arena, so we just have to allocate a new block, and then free the old one...
        void *new_pointer = this->allocate(new_user_size_in_bytes);
        memcpy(new_pointer, old_pointer, old_block->block_size_in_bytes);
        this->release(old_pointer);
        return new_pointer;
    }
}

void Memory_Pool::release(void *pointer) {
    Block_Header *block = this->get_header_from_user_pointer(pointer);
    assert(block->status == BLOCK_IN_USE && this->block_boundaries_look_valid(block));
    this->update_block_status(block, BLOCK_FREE);
    Block_Header *coalesced = this->maybe_coalesce_free_block(block);
    this->insert_block_into_free_list(coalesced);
}

u64 Memory_Pool::query_size(void *pointer) {
    Block_Header *block = this->get_header_from_user_pointer(pointer);
    assert(block->status == BLOCK_IN_USE && this->block_boundaries_look_valid(block) && block->user_size_in_bytes > 0);
    return block->user_size_in_bytes;
}

Allocator Memory_Pool::allocator() {
    Allocator allocator = {
        this,
        [](void *data, u64 size)      -> void* { return ((Memory_Pool *) data)->allocate(size); },
        [](void *data, void *pointer) -> void  { return ((Memory_Pool *) data)->release(pointer); },
        [](void *data, void *old_pointer, u64 new_size) -> void * { return ((Memory_Pool *) data)->reallocate(old_pointer, new_size); },
        [](void *data) -> void { ((Memory_Pool *) data)->destroy(); },
        [](void *data, void *pointer) -> u64   { return ((Memory_Pool *) data)->query_size(pointer); }
    };

    return allocator;
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
