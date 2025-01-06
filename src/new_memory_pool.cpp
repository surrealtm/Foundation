#include "new_memory_pool.h"
#include "os_specific.h"

namespace Ela { // nocheckin

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
    u64 index = os_highest_bit_set(transformed_size_in_bytes);
    return index;
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

};
