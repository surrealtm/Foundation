#include "concatenator.h"
#include "memutils.h"

#include <wchar.h>
#include <stdlib.h> // For mbstowcs

void Concatenator::create(Allocator *allocator, u64 block_size) {
    this->allocator   = allocator;
    this->block_size  = block_size;
    this->total_count = 0;
    this->last        = null;
    this->setup_block(&this->first);
}

void *Concatenator::finish() {
    if(this->total_count == 0) return null;

    if(this->last == &this->first) return this->first.data;

    u8 *buffer = (u8 *) this->allocator->allocate(this->total_count);
    s64 buffer_offset = 0;
    
    Block *block = &this->first;
    while(block) {
        memcpy(&buffer[buffer_offset], block->data, block->count);
        buffer_offset += block->count;
        this->allocator->deallocate(block->data);
        
        if(block != &this->first) {
            auto next = block->next;
            this->allocator->deallocate(block);
            block = next;
        } else {
            block = block->next;
        }
    }
    
    return buffer;
}


void Concatenator::add(const void *bytes, u64 count) {
    assert(this->last != null); // Check for use-before-create
    u64 offset = 0;
    
    while(offset < count) {
        if(this->last->count == this->last->capacity) this->append_block();
        
        s64 batch = min(count - offset, this->last->capacity - this->last->count);
        memcpy(&((u8 *) this->last->data)[this->last->count], &((u8 *) bytes)[offset], batch);
        this->last->count += batch;
        offset += batch;
    }

    this->total_count += count;
}

void Concatenator::add_unchecked(const void *bytes, u64 count) {
    assert(this->last != null); // Check for use-before-create
    assert(this->last->capacity - this->last->count >= count);

    memcpy(&((u8 *) this->last->data)[this->last->count], bytes, count);
    this->last->count += count;
    this->total_count += count;
}

void *Concatenator::reserve(u64 count, u8 alignment) {
    assert(count <= this->block_size);
    
    s64 required_padding = padding_to(this->total_count, alignment, s64);
    
    void *ptr;
    
    if(this->last->count + count + required_padding <= this->block_size) {
        this->last->count += required_padding;
        this->total_count += required_padding;
        ptr = &this->last->data[this->last->count];
    } else {
        this->append_block();
        this->last->count += required_padding;
        this->total_count += required_padding;
        ptr = &this->last->data[this->last->count];
    }

    this->last->count += count;
    this->total_count += count;
    
    return ptr;
}

s64 Concatenator::absolute_pointer_to_offset(void *pointer) {
    Block *block = &this->first;    
    s64 block_offset = 0;
    
    while(block != null && (pointer < block->data || pointer >= block->data + block->count)) {
        block_offset += block->count;
        block = block->next;
    }
    
    assert(block != null);
    
    return block_offset + (s64) ((u8 *) pointer - block->data);
}


void Concatenator::add_1b(u8 b) {
    this->add(&b, sizeof(u8));
}

void Concatenator::add_2b(u16 b) {
    this->add(&b, sizeof(u16));
}

void Concatenator::add_4b(u32 b) {
    this->add(&b, sizeof(u32));
}

void Concatenator::add_8b(u64 b) {
    this->add(&b, sizeof(u64));
}


void Concatenator::add_1b_unchecked(u8 b) {
    this->add_unchecked(&b, sizeof(u8));
}

void Concatenator::add_2b_unchecked(u16 b) {
    this->add_unchecked(&b, sizeof(u16));
}

void Concatenator::add_4b_unchecked(u32 b) {
    this->add_unchecked(&b, sizeof(u32));
}

void Concatenator::add_8b_unchecked(u64 b) {
    this->add_unchecked(&b, sizeof(u64));
}


void Concatenator::add_string(string value) {
    this->add(value.data, value.count);
}

void Concatenator::add_string_as_wide(string value) {
    wchar_t *pointer = (wchar_t *) temp.allocate(value.count * sizeof(wchar_t));
    u64 converted_characters = mbstowcs(pointer, (const char *) value.data, value.count);
    this->add(pointer, converted_characters * sizeof(wchar_t));
}

void Concatenator::add_wide_string(const wchar_t *value) {
    size_t length = wcslen(value);
    this->add(value, length * sizeof(wchar_t));
}


void Concatenator::modify(u64 offset, const void *bytes, u64 count) {
    // @@Speed: This could be sped up by copying as much as possible into one block, and then
    // going into the next one... This is just lazy.
    for(u64 i = 0; i < count; ++i) {
        this->modify_1b(offset + i, ((u8 *) bytes)[i]);
    }    
}

void Concatenator::modify_1b(u64 offset, u8 b) {
    //
    // :Modify
    // When modifying more than 1 byte, the concatenator needs to be careful that values might
    // cross Block borders. As an example, if an 8 byte pointer is added using add_8b(), then
    // the first two bytes might be in Block A (which is then full), the rest is spilled into
    // Block B, which requires additional checks when modifying a larger value.
    //
    
    Block *block = &this->first;
    u64 block_start = 0;
    while(block_start + block->count <= offset) {
        block_start += block->count;
        block = block->next;
    }
    
    u64 position_in_block = offset - block_start;
    block->data[position_in_block] = b;
}

void Concatenator::modify_2b(u64 offset, u16 b) {
    u8 *ptr = (u8 *) &b; // :Modify
    this->modify_1b(offset + 0, ptr[0]);
    this->modify_1b(offset + 1, ptr[1]);
}

void Concatenator::modify_4b(u64 offset, u32 b) {
    u8 *ptr = (u8 *) &b; // :Modify
    this->modify_1b(offset + 0, ptr[0]);
    this->modify_1b(offset + 1, ptr[1]);
    this->modify_1b(offset + 2, ptr[2]);
    this->modify_1b(offset + 3, ptr[3]);
}

void Concatenator::modify_8b(u64 offset, u64 b) {
    u8 *ptr = (u8 *) &b; // :Modify
    this->modify_1b(offset + 0, ptr[0]);
    this->modify_1b(offset + 1, ptr[1]);
    this->modify_1b(offset + 2, ptr[2]);
    this->modify_1b(offset + 3, ptr[3]);
    this->modify_1b(offset + 4, ptr[4]);
    this->modify_1b(offset + 5, ptr[5]);
    this->modify_1b(offset + 6, ptr[6]);
    this->modify_1b(offset + 7, ptr[7]);
}


void *Concatenator::mark() {
    assert(this->last != null);
    return &this->last->data[this->last->count];
}

void Concatenator::setup_block(Block *block) {
    block->capacity = this->block_size;
    block->data     = (u8 *) this->allocator->allocate(this->block_size);
    block->count    = 0;
    block->next     = null;

    if(this->last) this->last->next = block;

    this->last = block;
}

void Concatenator::append_block() {
    Block *block = (Block *) this->allocator->allocate(sizeof(Block));
    this->setup_block(block);
}
