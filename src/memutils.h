#pragma once

#include "foundation.h"

#define ONE_GIGABYTE (ONE_MEGABYTE * ONE_KILOBYTE)
#define ONE_MEGABYTE (ONE_KILOBYTE * ONE_KILOBYTE)
#define ONE_KILOBYTE (1024ULL)
#define ONE_BYTE     (1ULL)

#if FOUNDATION_DEVELOPER
# define ENABLE_ALLOCATOR_STATISTICS 1
#endif

enum Memory_Unit {
	Bytes,
	Kilobytes,
	Megabytes,
	Gigabytes,
	Terrabytes,
	MEMORY_UNIT_COUNT,
};

/* This codebases uses allocators for everything that requires memory management.
 * This allocator API abstracts over different memory allocation strategies, which
 * can then just be plugged in by using the appropriate allocator. If you don't care
 * about using a specific allocator, just use the Default_Allocator (which essentially
 * is like using malloc and free). */

typedef void*(*Allocate_Procedure)(void *data, u64 bytes);
typedef void(*Deallocate_Procedure)(void *data, void *pointer);
typedef void*(*Reallocate_Procedure)(void *data, void *old_pointer, u64 new_size);
typedef void(*Reset_Allocator_Procedure)(void *data);
typedef u64(*Query_Allocation_Size_Procedure)(void *data, void *pointer);

/* The allocator statistics provide insight into the memory usage of the application.
 * They display the activity of an allocator, show the total memory consumption and help
 * find memory leaks by counting allocations and deallocations. They do cause a little
 * overhead, so they should probably be deactivated in release builds. */
struct Allocator_Stats {
	u64 allocations; // The total number of allocations.
	u64 deallocations; // The total number of deallocations.
	u64 reallocations; // The number of reallocations done.
	u64 working_set; // The current total number of bytes that belong to active allocations.
	u64 peak_working_set; // The highest recorded working set.
};

struct Allocator {
	void *data; // The pointer to the underlying allocation strategy
	Allocate_Procedure   _allocate_procedure;
	Deallocate_Procedure _deallocate_procedure;
	Reallocate_Procedure _reallocate_procedure;
	Reset_Allocator_Procedure _reset_procedure;
	Query_Allocation_Size_Procedure _query_allocation_size_procedure;

#if ENABLE_ALLOCATOR_STATISTICS
	Allocator_Stats stats;
#endif


	void *allocate(u64 size); // Returns a new, zero-initialized blob of memory
	void deallocate(void *pointer); // Marks the memory pointer as free, so that it may be reused in the future. Not all allocation strategies support this.
	void *reallocate(void *old_pointer, u64 new_size); // Essentially deallocates the old pointer and allocates a new one based on the new size.
	void reset(); // Clears out the underlying allocation strategy
	void reset_stats(); // Clears out the allocation stats.
	u64 query_allocation_size(void *pointer); // Returns the original allocation size which returned this pointer. Not all allocation strategies support this.

	void debug_print(u32 indent);
};

/* The Heap Allocator, which just uses malloc under the hood, but also supports
 * allocator statistics by storing the allocation size inside the allocated block. */
void *heap_allocate(void *data /* = null */, u64 size);
void heap_deallocate(void *data /* = null */, void *pointer);
void *heap_reallocate(void *data /* = null */ , void *old_pointer, u64 new_size);
u64 heap_query_allocation_size(void *data /* = null */, void *pointer);

extern Allocator heap_allocator;
extern Allocator *Default_Allocator;

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
	void reset(); // Completely clears out this arena

	// Some memory allocations require a specific alignment, e.g. when working with
	// SIMD. This aligns the current size of the arena to the specified alignment,
	// potentially wasting a few bytes of memory. Note that this will mess with
	// allocator statistics.
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

	Allocator allocator();
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
	struct Block {
		u64 offset_to_next; // Offset in bytes to the next Block header.
		u64 size_in_bytes : 63; // Size in bytes of the usable data section of this block. Since the arena may be used by other things, this may not correspond to the offset to the next block.
		u64 used : 1; // Set to false once a block is freed, so that it may be merged or reused.
	
#if ENABLE_ALLOCATOR_STATISTICS
		u64 original_allocation_size; // The size_in_bytes of a block is not necessarily the "user"-requested size, e.g. for alignment or when merging blocks. This "user" size however is required for proper allocator statistics.
		u64 __padding; // The block header must be 16 byte aligned.
#endif

		Block *next();
		void *data();
		bool is_continuous_with(Block *block);
		void merge_with(Block *block);
	};

	static const u64 min_payload_size_to_split = 32; // The minimum data size in bytes for a block to make sense, meaning we won't split a block if the "left-over" data size is so small, that making a new block would not make sense.
	static_assert(sizeof(Memory_Pool::Block) % 16 == 0, "The Memory Pool Block was expected to be 16 byte aligned.");
	static_assert(Memory_Pool::min_payload_size_to_split >= sizeof(Memory_Pool::Block), "The aligned_block_size of the Memory_Pool is too little.");

	Memory_Arena *arena = null;
	Block *first_block  = null;
	Block *last_block   = null;

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
	void *reallocate(void *old_pointer, u64 new_size);

	u64 query_allocation_size(void *pointer);

	void debug_print(u32 indent = 0);

	// Sets up and returns an allocator based on this memory pool.
	Allocator allocator();
};

template<typename T>
struct Resizable_Array {
	static const s64 INITIAL_SIZE = 128;
	
	struct Iterator {
        T *pointer;
        
		bool operator!=(Iterator const &it) const { return this->pointer != it.pointer; }    
        Iterator &operator++() { ++this->pointer; return *this; }
        
        T *operator*()  { return this->pointer; }
		T *operator->() { return this->pointer; }
    };

	Allocator *allocator = Default_Allocator;
	T *data       = null;
	s64 count     = 0;
	s64 allocated = 0;
	
	void maybe_grow() {
		if(this->allocated == 0) {
			this->allocated = Resizable_Array::INITIAL_SIZE;
			this->data      = (T *) this->allocator->allocate(this->allocated * sizeof(T));
		} else if(this->count == this->allocated) {
			this->allocated *= 2;
		
			if(!this->allocator->_reallocate_procedure) {
				// Not all allocators actually provide a reallocation strategy (e.g. Memory Arenas). In that case,
				// allocate new memory manually, copy the existing data and if the allocator does have a deallocation
				// strategy, free the previous pointer. If the allocator does not have a deallocation, then it is
				// most likely some sort of scratch allocator that frees all memory at once.
				T *new_pointer = (T *) this->allocator->allocate(this->allocated * sizeof(T));
				memcpy(new_pointer, this->data, this->count * sizeof(T));
				if(this->allocator->_deallocate_procedure) this->allocator->deallocate(this->data);
				this->data = new_pointer;	
			} else {
				this->data = (T *) this->allocator->reallocate(this->data, this->allocated * sizeof(T));
			}
		}
	}

	void maybe_shrink() {
		if(this->count < this->allocated / 2 - 1 && this->allocated >= Resizable_Array::INITIAL_SIZE * 2) {
		    // If the array is less-than-half full, shrink the array
	        this->allocated /= 2;

			if(!this->allocator->_reallocate_procedure) {
				// Not all allocators actually provide a reallocation strategy (e.g. Memory Arenas). In that case,
				// allocate new memory manually, copy the existing data and if the allocator does have a deallocation
				// strategy, free the previous pointer. If the allocator does not have a deallocation, then it is
				// most likely some sort of scratch allocator that frees all memory at once.
				T *new_pointer = (T *) this->allocator->allocate(this->allocated * sizeof(T));
				memcpy(new_pointer, this->data, this->count * sizeof(T));
				if(this->allocator->_deallocate_procedure) this->allocator->deallocate(this->data);
				this->data = new_pointer;	
			} else {
				this->data = (T *) this->allocator->reallocate(this->data, this->allocated * sizeof(T));
			}
		}
	}

	void clear() {
		this->allocator->deallocate(this->data);
		this->count = 0;
		this->allocated = 0;
		this->data = null;
	}

	void add(T const &data) {
		this->maybe_grow();
		this->data[this->count] = data;
		++this->count;
	}

	void remove(s64 index) {
		memcpy(&this->data[index], &this->data[index + 1], (this->count - index) * sizeof(T));
		--this->count;
		this->maybe_shrink();
	}

	void remove_range(s64 first_to_remove, s64 last_to_remove) {
		memcpy(&this->data[first_to_remove], &this->data[last_to_remove + 1], (this->count - last_to_remove - 1) * sizeof(T));
		this->count -= (last_to_remove - first_to_remove) + 1;
		this->maybe_shrink();
	}

	void remove_by_value(T const &value) {
		for(s64 i = 0; i < this->count; ++i) {
			if(this->data[i] == value) {
				this->remove(i);
				break;
			}
		}
	}

	T *push() {
		this->maybe_grow();
		T *pointer = &this->data[this->count];
		++this->count;
		return pointer;
	}

	T pop() {
		assert(this->count > 0);
		T value = this->data[this->count - 1];
		--this->count;
		this->maybe_shrink();
		return value;
	}

	T &operator[](s64 index) {
		assert(index >= 0 && index < this->count);
		return this->data[index];
	}

    Iterator begin() { return Iterator { this->data }; }
	Iterator end() { return Iterator { this->data + this->count + 1 }; }
};

template<typename T>
struct Linked_List {
	struct Node {
		Node *next;
		T data;
	};

	struct Iterator {
        Node *pointer;
        
		bool operator!=(Iterator const &it) const { return this->pointer != it.pointer; }    
        Iterator &operator++() { this->pointer = this->pointer->next; return *this; }
        
        T *operator*()  { return &this->pointer->data; }
		T *operator->() { return &this->pointer->data; }
    };

	Allocator *allocator = Default_Allocator;
	Node *head = null;
	Node *tail = null;
	s64 count  = 0;

	Node *make_node(T const &value) {
		Node *node = (Node *) this->allocator->allocate(sizeof(Node));
		node->next = null;
		node->data = value;
		return node;
	}

	void add(T const &value) {
		Node *node = this->make_node(value);
		
		if(this->head) {
			this->tail->next = node;
			this->tail = node;
		} else {
			this->head = node;
			this->tail = this->head;
		}

		++this->count;
	}

	void remove(Node *node) {
		if(!node) return;

		if(node != this->head) {
			Node *previous = this->head;

			while(previous && previous->next != node) {
				previous = previous->next;
			}

			assert(previous != null);
			previous->next = node->next;

			if(this->tail == node) this->tail = previous;
		} else {
			this->head = this->head->next;
		}

		--this->count;
	}

	void remove(T const &value) {
		Node *node = this->head;

		while(node && node->value != value) {
			node = node->next;
		}

		this->remove(node);
	}

	void remove(s64 index) {
		assert(index >= 0 && index < this->count);
		
		Node *node = this->head;

		while(index > 0) {
			node = node->next;
			--index;
		}

		this->remove(node);
	}

	T *push() {
		this->add(T{});
		return &this->tail->data;
	}

	T pop() {
		assert(this->count > 0);

		Node *previous = this->head;

		while(previous && previous->next) {
			previous = previous->next;
		}

		T value = this->tail->data;
		this->tail = previous;
		return value;
	}

	T *first() { return &this->head->data; }

	T *last() { return &this->tail->data; }

	T &operator[](s64 index) {
		assert(index >= 0 && index < this->count);

		Node *node = this->head;

		while(index > 0) {
			node = node->next;
			--index;
		}

		return node->data;
	}

    Iterator begin() { return Iterator { this->head }; }
	Iterator end() { return Iterator { null }; }
};

const char *memory_unit_suffix(Memory_Unit unit);
Memory_Unit convert_to_biggest_memory_unit(s64 bytes, f32 *decimal);

void byteswap2(void *value);
void byteswap4(void *value);
void byteswap8(void *value);