#include "concatenator.h"
#include "memutils.h"

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


void Concatenator::add(void *bytes, u64 count) {
    assert(this->last != null); // Check for use-before-create
    u64 offset = 0;
    
    while(offset < count) {
        if(this->last->count == this->last->capacity) this->append_block();
        
        s64 batch = min(count, this->last->capacity - this->last->count);
        memcpy(&((u8 *) this->last->data)[this->last->count], &((u8 *) bytes)[offset], batch);
        this->last->count += batch;
        offset += count;
    }

    this->total_count += count;
}

void Concatenator::add_unchecked(void *bytes, u64 count) {
    assert(this->last != null); // Check for use-before-create
    assert(this->last->capacity - this->last->count >= count);

    memcpy(&((u8 *) this->last->data)[this->last->count], bytes, count);
    this->last->count += count;
    this->total_count += count;
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


void *Concatenator::mark() {
    assert(this->last != null);
    return &this->last[this->last->count];
}

void Concatenator::setup_block(Block *block) {
    block->capacity = this->block_size;
    block->data     = this->allocator->allocate(this->block_size);
    block->count    = 0;
    block->next     = null;

    if(this->last) {
        this->last->next = block;
    } else {
        this->last = block;
    }
}

void Concatenator::append_block() {
    Block *block = (Block *) this->allocator->allocate(sizeof(Block));
    this->setup_block(block);
}
