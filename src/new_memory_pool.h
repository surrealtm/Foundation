#include "foundation.h"
#include "memutils.h"

namespace Ela { // nocheckin

// This is a general-purpose memory allocator, similar to malloc.
// It is based on virtual memory, by internally creating and managing a memory arena.
// This allows exploiting the continuous layout of the reserved and committed
// virtual memory to have continuous blocks.
// Each block has a header and a footer (boundary tags), so that we can traverse
// the list of blocks from any user-pointer returned by the allocator at any time
// in any direction.
// The user-pointers are 16-byte aligned.
struct Memory_Pool {
private:
    static const s64 BIN_COUNT     = 16;
    static const s64 HEADER_SIZE   = 8;
    static const s64 FOOTER_SIZE   = 8;
    static const s64 METADATA_SIZE = HEADER_SIZE + FOOTER_SIZE;

    static const s64 BLOCK_IN_USE = 0;
    static const s64 BLOCK_FREE   = 1;

    struct Block_Header {
        // The header for all blocks. We store the block_size_in_bytes for internal handling
        // and the user size in bytes for allocator statistics...
        u32 block_size_in_bytes;
        u32 user_size_in_bytes: 31;
        u32 status:              1;
        
        // This data is stored in the payload for freed blocks. Since payloads are
        // always 16-byte aligned, they are guaranteed to be able to store these
        // two pointers.
        Block_Header *next_free;
        Block_Header *previous_free;
    };
    
    struct Block_Footer {
        // Similar to the block header, but stored just after the payload (instead of before),
        // so that we can access any block's previous block (as the footer of the previous block
        // is just before the header of the next block).
        u32 block_size_in_bytes;
        u32 user_size_in_bytes : 31;
        u32 status:               1;
    };
    
    struct Bin {
        Block_Header *first;
    };
    
    Memory_Arena arena;
    Bin bins[BIN_COUNT];

    inline b8 block_boundaries_look_valid(Block_Header *header);    
    inline b8 blocks_are_continuous(Block_Header *prev, Block_Header *next);
    inline void *get_user_pointer_from_header(Block_Header *header);
    inline Block_Footer *get_footer_from_header(Block_Header *header);
    inline Block_Header *get_header_from_footer(Block_Footer *footer);
    inline Block_Header *get_header_from_user_pointer(void *user_pointer);
    inline Block_Footer *get_footer_from_user_pointer(void *user_pointer);
    inline Block_Header *get_next_header_from_header(Block_Header *header);
    inline Block_Header *get_previous_header_from_header(Block_Header *header);
    inline Block_Header *split_block(Block_Header *existing, u64 existing_block_size_in_bytes, u64 existing_user_size_in_bytes);
    inline void update_block_size_in_bytes(Block_Header *header, u64 block_size_in_bytes, u64 user_size_in_bytes);
    inline void update_block_status(Block_Header *header, u64 status);
    
    inline s64 get_bin_index_for_size(u64 aligned_size_in_bytes);
    inline void insert_block_into_free_list(Block_Header *free_block);
    inline void remove_block_from_free_list(Block_Header *free_block);
    Block_Header *maybe_coalesce_free_block(Block_Header *free_block);

public:
    
    void create(u64 reserved, u64 requested_commit_size = 0);
    void destroy();
    void reset();
    
    void *allocate(u64 user_size_in_bytes);
    void *reallocate(void *old_pointer, u64 new_user_size_in_bytes);
    void release(void *pointer);
    u64 query_size(void *pointer);
    
    Allocator allocator();
};

};