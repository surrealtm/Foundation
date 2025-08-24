#pragma once

#include "foundation.h"
#include "string_type.h"

struct Allocator;

struct Concatenator {
    struct Block {
        Block *next;
        u8 *data;
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

    void add(const void *bytes, u64 count);
    void add_unchecked(const void *bytes, u64 count);

    void *reserve(u64 count, u8 alignment);
    s64 absolute_pointer_to_offset(void *pointer);

    void add_1b(u8 b);
    void add_2b(u16 b);
    void add_4b(u32 b);
    void add_8b(u64 b);

    void add_1b_unchecked(u8 b);
    void add_2b_unchecked(u16 b);
    void add_4b_unchecked(u32 b);
    void add_8b_unchecked(u64 b);
    
    void add_string(string value);
    void add_string_as_wide(string value);
    void add_wide_string(const wchar_t *value);

    void clear_range(u64 offset, u64 count);
    void modify(u64 offset, const void *bytes, u64 count);

    void modify_1b(u64 offset, u8 b);
    void modify_2b(u64 offset, u16 b);
    void modify_4b(u64 offset, u32 b);
    void modify_8b(u64 offset, u64 b);
    
    void *get_trailing_pointer();
    
    void setup_block(Block *block);
    void append_block();
    Block *find_block_for_offset(u64 offset, u64 *position_in_block);
};
