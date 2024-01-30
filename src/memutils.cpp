#include "memutils.h"
#include "os_specific.h"

#include <stdlib.h>

Allocator heap_allocator = { null, heap_allocate, heap_deallocate, heap_query_allocation_size };
Allocator *Default_Allocator = &heap_allocator;

/* ============================ ALLOCATOR ============================ */

void *Allocator::allocate(u64 size) {
	// @Incomplete: Statistics
	return this->_allocate_procedure(this->data, size);
}

void Allocator::deallocate(void *pointer) {
	// @Incomplete: Statistics
	this->_deallocate_procedure(this->data, pointer);
}

u64 Allocator::query_allocation_size(void *pointer) {
	return this->_query_allocation_size_procedure(this->data, pointer);
}

/* ============================ HEAP ALLOCATOR ============================ */

void *heap_allocate(void *data /* = null */, u64 size) {
	void *pointer = null;

#if ENABLE_ALLOCATOR_STATISTICS
	// To support allocator statistics at runtime, we need to store the allocation
	// size somewhere. While malloc does this somewhere under the hood, we don't
	// have access to that information, so instead we need to store that size
	// ourselves. We do that by simply allocating a bigger block, and writing the
	// size at the first few bytes of the returned block. Not super elegent, but
	// better than the alternatives.
	u64 extra_size = align_to(sizeof(u64), 16, u64); // Stuff like SIMD sometimes requires 16-byte alignment...
	pointer = malloc(extra_size + size);
	if(!pointer) {
		assert(0 && "malloc failed.");
		return null;
	}

	u64 *_u64 = (u64 *) pointer;
	*_u64 = size;
	pointer = (void *) ((u64) pointer + extra_size);
#else
	pointer = malloc(size); // @Cleanup: Maybe we can even use Platform specific stuff here, like HeapAlloc?
#endif

	return pointer;
}

void heap_deallocate(void *data /* = null */, void *pointer) {
	if(!pointer) return;

#if ENABLE_ALLOCATOR_STATISTICS
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

u64 heap_query_allocation_size(void *data /* = null */, void *pointer) {
	u64 size;

#if ENABLE_ALLOCATOR_STATISTICS
	u64 extra_size = align_to(sizeof(u64), 16, u64);
	u64 *_u64 = (u64 *) ((u64) pointer - extra_size);
	size = *_u64;
#else
	assert(false && "ENABLE_ALLOCATOR_STATISTICS IS OFF, heap_query_allocation_size is unsupported."); // @Cleanup: Add a custom foundation_assert thing.
	size = 0;
#endif

	return size;
}


/* ============================ MEMORY ARENA ============================ */

void Memory_Arena::create(u64 reserved, u64 requested_commit_size) {
	assert(this->base == null);
	assert(reserved != 0);
	
	if(this->base = os_reserve_memory(reserved)) {
		this->page_size   = os_get_page_size();
		this->commit_size = requested_commit_size ? align_to(requested_commit_size, this->page_size, u64) : this->page_size * 3;
		this->reserved    = reserved;
		this->committed   = 0;
		this->size        = 0;
	} else {
		this->page_size   = 0;
		this->commit_size = 0;
		this->reserved    = 0;
		this->committed   = 0;
		this->size        = 0;
	}
}

void Memory_Arena::destroy() {
	assert(this->base != null); // An arena can only be destroyed once. The caller needs to ensure it has not been cleaned up yet.
	assert(this->reserved != 0);
	os_free_memory(this->base, this->reserved);
	this->base = null;
	this->size = 0;
	this->reserved = 0;
	this->committed = 0;
	this->page_size = 0;
	this->commit_size = 0;
}

u64 Memory_Arena::ensure_alignment(u64 alignment) {
	u64 padding = align_to(this->size, alignment, u64) - this->size;
	this->push(padding);
	return padding;
}

void *Memory_Arena::push(u64 size) {
	assert(this->base != null); // Make sure the arena is set up properly.

	if(this->size + size > this->committed) {
		if(this->committed + this->commit_size <= this->reserved) {
			if(os_commit_memory((char *) this->base + this->committed, this->commit_size)) {
				this->committed += this->commit_size;
			} else {
				this->destroy();
				return null;
			}
		} else {
			report_error("The Memory_Arena ran out of reserved space (" PRIu64 "b reserved, " PRIu64 "b committed).", this->reserved, this->committed);
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
	
	os_decommit_memory((char *) this->base + this->committed - decommit_size, decommit_size);
	this->committed -= decommit_size;
}

void Memory_Arena::debug_print(u32 indent) {
	// @Cleanup: Better format the bytes count here.
	printf("%-*s=== Memory Arena ===\n", indent, "");
	printf("%-*s    Reserved:    %" PRIu64 "b.\n", indent, "", this->reserved);
	printf("%-*s    Committed:   %" PRIu64 "b.\n", indent, "", this->committed);
	printf("%-*s    Size:        %" PRIu64 "b.\n", indent, "", this->size);
	printf("%-*s    Commit-Size: %" PRIu64 "b.\n", indent, "", this->commit_size);
	printf("%-*s    (OS-Committed Region: %" PRIu64 "b.)\n", indent, "", os_get_committed_region_size(this->base));
	printf("%-*s=== Memory Arena ===\n", indent, "");
}


/* ============================ MEMORY POOL ============================ */

Memory_Pool::Block *Memory_Pool::Block::next() {
	if(this->offset_to_next == 0) return null;

	return (Block *) ((char *) this + this->offset_to_next);
}

void *Memory_Pool::Block::data() {
	return (char *) this + Memory_Pool::aligned_block_size;
}

bool Memory_Pool::Block::is_continuous_with(Block *block) {
	return ((char *) this->data() + this->size_in_bytes) == (char *) block;
}

void Memory_Pool::Block::merge_with(Block *block) {
	if(block->offset_to_next) {
		u64 next_block = (u64) block + block->offset_to_next;
		this->offset_to_next = next_block - (u64) this;
	} else
		this->offset_to_next = 0;

	this->size_in_bytes += block->size_in_bytes + Memory_Pool::aligned_block_size;
}

void Memory_Pool::create(Memory_Arena *arena) {
	this->arena = arena;
	this->first_block = null;
}

void Memory_Pool::destroy() {
	this->first_block = null;
}

void *Memory_Pool::push(u64 size) {
	assert(size < 0x7fffffffffffffff); // Make sure we only require 63 bits for the size, or else our Block struct cannot properly encode it.

	//
	// Query the Memory Pool for an inactive block that can be reused for this
	// allocation.
	//
	Block *unused_block = this->first_block;

	while(unused_block != null && (unused_block->used || unused_block->size_in_bytes < size)) {
		unused_block = unused_block->next();
	}

	void *data = null;

	if(unused_block) {
		//
		// There exist an unused block that is big enough for this allocation to reuse it.
		//
		if(unused_block->size_in_bytes - size >= Memory_Pool::min_size_to_split + Memory_Pool::aligned_block_size) {
			//
			// Split the existing block into two parts, and only use the first one.
			//
			u64 split_start = align_to((u64) unused_block->data() + size, 16, u64);
		
			Block *split_block = (Block *) split_start;
			split_block->offset_to_next = unused_block->offset_to_next - (split_start - (u64) unused_block);
			split_block->size_in_bytes  = unused_block->size_in_bytes - split_start - Memory_Pool::aligned_block_size;
			split_block->used           = false;

			unused_block->offset_to_next = (u64) split_block - (u64) unused_block;
			unused_block->size_in_bytes  = split_start - (u64) unused_block - Memory_Pool::aligned_block_size;
		}
		
		//
		// Mark this block as used now.
		//
		unused_block->used = true;
		data = unused_block->data();
	} else {
		//
		// The Memory Pool does not have any unused block that could be reused for
		// this allocation.
		// Allocate a new block and set it and add it to the list.
		//

		//
		// Ensure alignment for this new block. Just increasing the arena's data size
		// would make the memory blocks non-continuous (disabling block merging on free), 
		// therefore check if we are currently continuous, and then manually modify the 
		// size_in_bytes of the previous block if we know that the only push on the 
		// arena was us.
		//
		if(this->last_block != null &&
			(char *) this->last_block->data() + this->last_block->size_in_bytes == (char *) this->arena->base + this->arena->size) {
			u64 padding = this->arena->ensure_alignment(16);
			assert(size < 0x7fffffffffffffef); // Make sure adding the padding won't overflow the size.
			this->last_block->size_in_bytes += padding;
		} else
			this->arena->ensure_alignment(16);
		
		void *pointer = this->arena->push(Memory_Pool::aligned_block_size + size);
		
		Block *block = (Block *) pointer;
		block->offset_to_next = 0;
		block->size_in_bytes  = size;
		block->used           = true;
		
		if(this->last_block) {
			assert(this->first_block != null);
			assert(this->last_block->offset_to_next == 0);
			this->last_block->offset_to_next = (char *) block - (char *)this->last_block;
			this->last_block = block;
		} else {
			this->first_block = block;
			this->last_block = block;
		}
	
		data = block->data();
	}

	return data;
}

void Memory_Pool::release(void *pointer) {
	// Silently ignore 'null' released.
	if(!pointer) return;

	//
	// Find the block that corresponds to this pointer.
	//
	Block *previous_block = null;
	Block *block = this->first_block;
	while(block && (char *) block + Memory_Pool::aligned_block_size < pointer) {
		previous_block = block;
		block = block->next();
	}

	if(!block || block->data() != pointer) {
		// The pointer is invalid, it does not correspond to a block.
		report_error("Attempted to release pointer from Memory_Pool which does not correspond to a block.");
		return;
	}

	//
	// Release this block. If possible, merge with the previous and the next block
	// so that the block list stays as small as possible.
	//
	block->used = false;
	
	if(previous_block && !previous_block->used && previous_block->is_continuous_with(block)) {
		//
		// Merge with the previous block.
		//
		previous_block->merge_with(block);
		block = previous_block;
	}

	Block *next = block->next();
	if(next && !next->used && block->is_continuous_with(next)) {
		//
		// Merge with the next block.
		//
		block->merge_with(next);
	}
}

void Memory_Pool::debug_print(u32 indent) {
	printf("%-*s=== Memory Pool ===\n", indent, "");

	printf("%-*s  Underlying Arena:\n", indent, "");
	this->arena->debug_print(indent + 4);

	printf("%-*s  Pool Blocks:\n", indent, "");

	if(this->first_block) {
		u64 index = 0;
		Block *block = this->first_block;
		while(block) {
			u64 offset = (char *) block - (char *) this->arena->base;
			printf("%-*s    > %" PRIu64 ": %" PRIu64 "b, %s. (Offset: %" PRIu64 ").\n", indent, "", index, block->size_in_bytes, block->used ? "Used" : "Free", offset);
			block = block->next();
			++index;
		}
	} else
		printf("%-*s    (Empty Pool).\n", indent, "");

	printf("%-*s=== Memory Pool ===\n", indent, "");
}