#pragma once

#include "foundation.h"

#define ONE_GIGABYTE (ONE_MEGABYTE * ONE_MEGABYTE)
#define ONE_MEGABYTE (ONE_KILOBYTE * ONE_KILOBYTE)
#define ONE_KILOBYTE (1024ULL)
#define ONE_BYTE     (1ULL)

/* A memory arena (also known as a linear allocator) is just a big block of
 * reserved virtual memory, that gradually commits to physical memory as it
 * grows. A memory arena just pushes its head further along for every allocation
 * and does not store any additional information.
 * This makes it a lightweight base allocator for when deallocation is not
 * necessary.
 * A memory arena guarantees zero-initialized memory to be returned on push.
 * A memory arena may be reset to a certain watermark, decommitting any now-unused
 * pages (and therefore invalidating all allocations that came after the watermark).
 */
struct Memory_Arena {
	void *base      = null;
	u64 commit_size = 0;
	u64 page_size   = 0;
	u64 committed   = 0;
	u64 reserved    = 0;
	u64 size        = 0;
	
	void create(u64 reserved, u64 requested_commit_size = 0);
	void destroy();

	// Some memory allocations require a specific alignment, e.g. when working with
	// SIMD. This aligns the current size of the arena to the specified alignment,
	// potentially wasting a few bytes of memory.
	u64 ensure_alignment(u64 alignment);
	void *push(u64 size);

	// Returns the current size of the memory arena. This value can then be used 
	// to reset the arena to that position, to decommit any allocations done 
	// since the mark was queried
	u64 mark();
	// Resets the arena's size to the supplied mark. The arena attempts to 
	// decommit as many pages as possible that are no longer required.
	void release_from_mark(u64 mark);

	void debug_print(u32 indent = 0);
};

/* A memory pool sits on top of a memory arena and stores metadata about each
 * allocation alongside the actual committed data. This enables the pool to
 * store a free-list of space that may be reused in future allocations, after
 * the initial allocation has been released.
 * The arena may be shared between this pool and any other user of the arena,
 * however releasing / shrinking the arena must of course be synced with this pool.
 * A memory pool guarantees zero-initialized memory to be returned on push.
 */
struct Memory_Pool {
	// This is the block header that gets inlined in an allocation block, to
	// be maintained as a list over all free / active blocks.
	// @@Speed: Make this block smaller, if possible, by merging some members?
	struct Block {
		u64 offset_to_next; // Offset in bytes to the next Block header.
		u64 size_in_bytes; // Size in bytes of the usable data section of this block. Since the arena may be used by other things, this may not correspond to the offset to the next block.
		b8 used; // Set to false once a block is freed, so that it may be merged or reused.
	
		Block *next();
		void *data();
		bool is_continuous_with(Block *block);
		void merge_with(Block *block);
	};

	static const u64 min_size_to_split  = 32; // The minimum data size in bytes for a block to make sense, meaning we won't split a block if the "left-over" data size is so small, that making a new block would not make sense.
	static const u64 aligned_block_size = 32;
	static_assert(Memory_Pool::aligned_block_size >= sizeof(Block), "The aligned_block_size of the Memory_Pool is too little.");

	Memory_Arena *arena = null;
	Block *first_block = null;
	Block *last_block = null;

	// Sets up an empty memory pool. Since a memory pool shares the arena, it is not the responsibility
	// of the pool to manage the arena.
	void create(Memory_Arena *arena);

	// Clears out the internal representation of the allocation list.
	// Since the memory pool shares the arena, it will not manipulate the arena here in any way.
	// Eventually destroying the arena is the responsibility of whatever created the arena in
	// the first place.
	void destroy();

	void *push(u64 size);
	void release(void *pointer);

	void debug_print(u32 indent = 0);
};