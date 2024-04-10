#include "memutils.h"
#include "os_specific.h"

#include <stdlib.h>

Allocator heap_allocator = { null, heap_allocate, heap_deallocate, heap_reallocate, null, heap_query_allocation_size };
Allocator *Default_Allocator = &heap_allocator;



/* ------------------------------------------------ Allocator ------------------------------------------------ */

void *Allocator::allocate(u64 size) {
#if ENABLE_ALLOCATOR_STATISTICS
	++this->stats.allocations;
	this->stats.working_set += size;
	if(this->stats.working_set > this->stats.peak_working_set) this->stats.peak_working_set = this->stats.working_set;
#endif

	return this->_allocate_procedure(this->data, size);
}

void Allocator::deallocate(void *pointer) {
	if(pointer == null) return; // Silently ignore "null" deallocations

#if ENABLE_ALLOCATOR_STATISTICS
	++this->stats.deallocations;
	u64 size = this->_query_allocation_size_procedure(this->data, pointer);
	this->stats.working_set -= size;
#endif

	this->_deallocate_procedure(this->data, pointer);
}

void *Allocator::reallocate(void *old_pointer, u64 new_size) {
	if(old_pointer == null) return null; // Silently ignore "null" deallocations
	
#if ENABLE_ALLOCATOR_STATISTICS
	++this->stats.reallocations;
	u64 previous_size = this->_query_allocation_size_procedure(this->data, old_pointer);
	this->stats.working_set += (new_size - previous_size);
	if(this->stats.working_set > this->stats.peak_working_set) this->stats.peak_working_set = this->stats.working_set;
#endif

	return this->_reallocate_procedure(this->data, old_pointer, new_size);
}

void Allocator::reset() {
	this->_reset_procedure(this->data);
	this->reset_stats();
}

void Allocator::reset_stats() {
#if ENABLE_ALLOCATOR_STATISTICS
	this->stats.allocations      = 0;
	this->stats.deallocations    = 0;
	this->stats.reallocations    = 0;
	this->stats.working_set      = 0;
	this->stats.peak_working_set = 0;
#endif
}

u64 Allocator::query_allocation_size(void *pointer) {
	return this->_query_allocation_size_procedure(this->data, pointer);
}

void Allocator::debug_print(u32 indent) {
#if ENABLE_ALLOCATOR_STATISTICS
	f32 working_set_decimal, peak_working_set_decimal;
	Memory_Unit working_set_unit, peak_working_set_unit;

	working_set_unit = convert_to_biggest_memory_unit(this->stats.working_set, &working_set_decimal);
	peak_working_set_unit = convert_to_biggest_memory_unit(this->stats.peak_working_set, &peak_working_set_decimal);
	
	printf("%-*s=== Allocator ===\n", indent, "");
	printf("%-*s    Allocations:      %lld.\n", indent, "", this->stats.allocations);
	printf("%-*s    Deallocations:    %lld.\n", indent, "", this->stats.deallocations);
	printf("%-*s     -> Alive:        %lld.\n", indent, "", this->stats.allocations - this->stats.deallocations);
	printf("%-*s    Reallocations:    %lld.\n", indent, "", this->stats.reallocations);
	printf("%-*s    Working Set:      %.3f%s.\n", indent, "", working_set_decimal, memory_unit_suffix(working_set_unit));
	printf("%-*s    Peak Working Set: %.3f%s.\n", indent, "", peak_working_set_decimal, memory_unit_suffix(peak_working_set_unit));
	printf("%-*s=== Allocator ===\n", indent, "");
#endif
}



/* ---------------------------------------------- Heap Allocator ---------------------------------------------- */

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
		report_error("A call to malloc failed for the requested size of '%lld'.", size);
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

void *heap_reallocate(void *data /* = null */, void *old_pointer, u64 new_size) {
	return realloc(old_pointer, new_size);
}

u64 heap_query_allocation_size(void *data /* = null */, void *pointer) {
	u64 size;

#if ENABLE_ALLOCATOR_STATISTICS
	u64 extra_size = align_to(sizeof(u64), 16, u64);
	u64 *_u64 = (u64 *) ((u64) pointer - extra_size);
	size = *_u64;
#else
	report_error("ENABLE_ALLOCATOR_STATISTICS is off, heap_query_allocation_size is unsupported.");
	size = 0;
#endif

	return size;
}



/* ----------------------------------------------- Memory Arena ----------------------------------------------- */

void Memory_Arena::create(u64 reserved, u64 requested_commit_size) {
	assert(this->base == null);
	assert(reserved != 0);
	
	if(this->base = os_reserve_memory(reserved)) {
		this->page_size   = os_get_page_size();
		this->commit_size = requested_commit_size ? align_to(requested_commit_size, this->page_size, u64) : this->page_size * 3;
		this->reserved    = align_to(reserved, this->page_size, u64);
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
	this->base        = null;
	this->size        = 0;
	this->reserved    = 0;
	this->committed   = 0;
	this->page_size   = 0;
	this->commit_size = 0;
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

	if(decommit_size) {
		os_decommit_memory((char *) this->base + this->committed - decommit_size, decommit_size);
		this->committed -= decommit_size;
	}
}

void Memory_Arena::debug_print(u32 indent) {
	f32 reserved_decimal, committed_decimal, size_decimal, commit_size_decimal, os_region_decimal;
	Memory_Unit reserved_unit, committed_unit, size_unit, commit_size_unit, os_region_unit;

	reserved_unit    = convert_to_biggest_memory_unit(this->reserved, &reserved_decimal);
	committed_unit   = convert_to_biggest_memory_unit(this->committed, &committed_decimal);
	size_unit        = convert_to_biggest_memory_unit(this->size, &size_decimal);
	commit_size_unit = convert_to_biggest_memory_unit(this->commit_size, &commit_size_decimal);
	os_region_unit   = convert_to_biggest_memory_unit(os_get_committed_region_size(this->base), &os_region_decimal);

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

Memory_Pool::Block *Memory_Pool::Block::next() {
	if(this->offset_to_next == 0) return null;

	return (Block *) ((char *) this + this->offset_to_next);
}

void *Memory_Pool::Block::data() {
	return (char *) this + sizeof(Memory_Pool::Block);
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

	this->size_in_bytes += block->size_in_bytes + sizeof(Memory_Pool::Block);
}

void Memory_Pool::create(Memory_Arena *arena) {
	this->arena = arena;
	this->first_block = null;
}

void Memory_Pool::destroy() {
	this->first_block = null;
}

void *Memory_Pool::push(u64 size) {
	assert(size <= 0x7fffffffffffffff); // Make sure we only require 63 bits for the size, or else our Block struct cannot properly encode it.

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
		u64 reused_size = align_to(size, 16, u64);
		
		if(unused_block->size_in_bytes - reused_size >= Memory_Pool::min_payload_size_to_split + sizeof(Memory_Pool::Block)) {
			//
			// Split the existing block into two parts, and only use the first one.
			//
			u64 split_size  = unused_block->size_in_bytes - reused_size - sizeof(Memory_Pool::Block);
			u64 split_start = align_to((u64) unused_block->data() + size, 16, u64);
		
			Block *split_block = (Block *) split_start;
			if(unused_block->offset_to_next)
				split_block->offset_to_next = (u64) unused_block + unused_block->offset_to_next - split_start;
			else
				split_block->offset_to_next = 0;
			
			split_block->size_in_bytes  = split_size;
			split_block->used           = false;

			unused_block->offset_to_next = (u64) split_block - (u64) unused_block;
			unused_block->size_in_bytes  = reused_size;
		
			if(this->last_block == unused_block) this->last_block = split_block;
		}
		
		//
		// Mark this block as used now.
		//
		unused_block->used = true;
		
#if ENABLE_ALLOCATOR_STATISTICS
		unused_block->original_allocation_size = size;
#endif

		data = unused_block->data();
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
		if(this->last_block != null &&
			(char *) this->last_block->data() + this->last_block->size_in_bytes == (char *) this->arena->base + this->arena->size) {
			u64 padding = this->arena->ensure_alignment(16);
			assert(size < 0x7fffffffffffffef); // Make sure adding the padding won't overflow the size.
			this->last_block->size_in_bytes += padding;
		} else
			this->arena->ensure_alignment(16);
		
		void *pointer = this->arena->push(sizeof(Memory_Pool::Block) + size);
		
		Block *block = (Block *) pointer;
		block->offset_to_next = 0;
		block->size_in_bytes  = size;
		block->used           = true;

#if ENABLE_ALLOCATOR_STATISTICS
		block->original_allocation_size = size;
#endif
		
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
	// Silently ignore 'null' releases.
	if(!pointer) return;

	//
	// Find the block that corresponds to this pointer.
	//
	Block *previous_block = null;
	Block *block = this->first_block;
	while(block && block->data() < pointer) {
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
		if(block == this->last_block) this->last_block = previous_block;
		previous_block->merge_with(block);
		block = previous_block;
	}

	Block *next = block->next();
	if(next && !next->used && block->is_continuous_with(next)) {
		//
		// Merge with the next block.
		//
		if(next == this->last_block) this->last_block = block;
		block->merge_with(next);
	}
}

void *Memory_Pool::reallocate(void *old_pointer, u64 new_size) {
    // This is definitely not a very smart reallocation technique, but it has to do for now.
    // In the future, it would make sense to only actually allocate a new block if the new size is bigger than
    // the old one. If the new size is smaller, the current block could be split to free up space.
    u64 old_size = this->query_allocation_size(old_pointer);
    void *new_pointer = this->push(new_size);

    memcpy(new_pointer, old_pointer, min(old_size, new_size));
    this->release(old_pointer);
    return new_pointer;
}

u64 Memory_Pool::query_allocation_size(void *pointer) {
#if ENABLE_ALLOCATOR_STATISTICS
	Block *block = this->first_block;
	while(block && block->data() < pointer) {
		block = block->next();
	}

	assert(block != null && block->data() == pointer);
	assert(block->used);

	return block->original_allocation_size;
#else
	return 0;
#endif
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

Allocator Memory_Pool::allocator() {
	Allocator allocator = {
		this, 
		[](void *data, u64 size)      -> void* { return ((Memory_Pool *) data)->push(size); },
		[](void *data, void *pointer) -> void  { return ((Memory_Pool *) data)->release(pointer); },
		[](void *data, void *old_pointer, u64 new_size) -> void * { return ((Memory_Pool*) data)->reallocate(old_pointer, new_size); },
		[](void *data) -> void { ((Memory_Pool*) data)->destroy(); },
		[](void *data, void *pointer) -> u64   { return ((Memory_Pool *) data)->query_allocation_size(pointer); } 
	};

	return allocator;
}



/* -------------------------------------------------- Utils -------------------------------------------------- */

const char *memory_unit_suffix(Memory_Unit unit) {
	const char *string = "(UnknownMemoryUnit)";

	switch(unit) {
	case Bytes:      string = "b"; break;
	case Kilobytes:  string = "kb"; break;
	case Megabytes:  string = "mb"; break;
	case Gigabytes:  string = "gb"; break;
	case Terrabytes: string = "tb"; break;
	}

	return string;
}

Memory_Unit convert_to_biggest_memory_unit(s64 bytes, f32 *decimal) {
	Memory_Unit unit = Bytes;
	*decimal = (f32) bytes;

	while(unit < MEMORY_UNIT_COUNT && bytes >= 1000) {
		unit = (Memory_Unit) (unit + 1);
		*decimal = (f32) bytes / 1000.0f;
		bytes /= 1000;
	}
		
	return unit;
}
