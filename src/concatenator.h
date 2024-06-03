#pragma once
#include "foundation.h"

struct Allocator;

struct Concatenator {
    struct Block {
        Block *next;
        void *data;
        u64 capacity;
        u64 count;
    };

    Allocator *allocator;
    Block first;
    Block *last;
    u64 total_count;
    u64 block_size;
    
    void create(Allocator *allocator, u64 block_size);
    void *finish();

    void add(void *bytes, u64 count);
    void add_unchecked(void *bytes, u64);

    void add_1b(u8 b);
    void add_2b(u16 b);
    void add_4b(u32 b);
    void add_8b(u64 b);

    void add_1b_unchecked(u8 b);
    void add_2b_unchecked(u16 b);
    void add_4b_unchecked(u32 b);
    void add_8b_unchecked(u64 b);

    void *mark();
    
    void setup_block(Block *block);
    void append_block();
};
