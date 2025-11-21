//
// Careful:
// This source file gets #include'd in the header file, because templates are shit!
//



/* --------------------------------------------- Resizable Array --------------------------------------------- */

template<typename T>
void Resizable_Array<T>::maybe_grow(b8 force) {
    if(!this->data) {
        if(this->allocated == 0) this->allocated = Resizable_Array::INITIAL_SIZE;
        this->data      = (T *) this->allocator->allocate(this->allocated * sizeof(T));
    } else if(force || this->count == this->allocated) {
        if(!force) this->allocated *= 2;
		
        if(!this->allocator->_reallocate_procedure) {
            // Not all allocators actually provide a reallocation strategy (e.g. Memory Arenas). In that case,
            // allocate new memory manually, copy the existing data and if the allocator does have a
            // deallocation strategy, free the previous pointer. If the allocator does not have a
            // deallocation, then it is most likely some sort of scratch allocator that frees all memory at
            // once.
            T *new_pointer = (T *) this->allocator->allocate(this->allocated * sizeof(T));
            memmove(new_pointer, this->data, this->count * sizeof(T));
            if(this->allocator->_deallocate_procedure) this->allocator->deallocate(this->data);
            this->data = new_pointer;	
        } else {
            this->data = (T *) this->allocator->reallocate(this->data, this->allocated * sizeof(T));
        }
    }
}

template<typename T>
void Resizable_Array<T>::maybe_shrink() {
    if(this->count < this->allocated / 2 - 1 && this->allocated >= Resizable_Array::INITIAL_SIZE * 2) {
        // If the array is less-than-half full, shrink the array
        this->allocated /= 2;
        assert(this->count > 0 && this->count <= this->allocated);

        if(!this->allocator->_reallocate_procedure) {
            // Not all allocators actually provide a reallocation strategy (e.g. Memory Arenas). In that case,
            // allocate new memory manually, copy the existing data and if the allocator does have a
            // deallocation strategy, free the previous pointer. If the allocator does not have a
            // deallocation, then it is most likely some sort of scratch allocator that frees all memory at
            // once.
            T *new_pointer = (T *) this->allocator->allocate(this->allocated * sizeof(T));
            memmove(new_pointer, this->data, this->count * sizeof(T));
            if(this->allocator->_deallocate_procedure) this->allocator->deallocate(this->data);
            this->data = new_pointer;	
        } else {
            this->data = (T *) this->allocator->reallocate(this->data, this->allocated * sizeof(T));
        }
    }
}

template<typename T>
void Resizable_Array<T>::clear() {
    this->allocator->deallocate(this->data);
    this->count     = 0;
    this->allocated = 0;
    this->data      = null;
}

template<typename T>
void Resizable_Array<T>::clear_without_deallocation() {
    this->count     = 0;
    this->allocated = 0;
    this->data      = null;
}

template<typename T>
void Resizable_Array<T>::reserve(s64 count) {
    assert(count >= 0);
    if(this->allocated == 0) this->allocated = Resizable_Array::INITIAL_SIZE;
    s64 least_size = this->allocated + count;
    while(this->allocated < least_size) this->allocated *= 2;
    if(count > 0) this->maybe_grow(true);    
}

template<typename T>
void Resizable_Array<T>::reserve_exact(s64 count) {
    assert(count >= 0);
    this->allocated = this->allocated + count;
    if(count > 0) this->maybe_grow(true);
}

template<typename T>
void Resizable_Array<T>::add(T const &data) {
    this->maybe_grow();
    this->data[this->count] = data;
    ++this->count;
}

template<typename T>
void Resizable_Array<T>::insert(s64 index, T const &data) {
    assert(index >= 0 && index <= this->count);
    this->maybe_grow();
    if(index < this->count) memmove(&this->data[index + 1], &this->data[index], (this->count - index) * sizeof(T));
    this->data[index] = data;
    ++this->count;
}

template<typename T>
void Resizable_Array<T>::remove(s64 index) {
    assert(index >= 0 && index < this->count);
    memmove(&this->data[index], &this->data[index + 1], (this->count - index - 1) * sizeof(T));
    --this->count;
    this->maybe_shrink();
}

template<typename T>
void Resizable_Array<T>::remove_range(s64 first_to_remove, s64 last_to_remove) {
    assert(first_to_remove >= 0 && first_to_remove < this->count);
    assert(last_to_remove >= 0 && last_to_remove < this->count);
    assert(last_to_remove >= first_to_remove);
    memmove(&this->data[first_to_remove], &this->data[last_to_remove + 1], (this->count - last_to_remove - 1) * sizeof(T));
    this->count -= (last_to_remove - first_to_remove) + 1;
    this->maybe_shrink();
}

template<typename T>
void Resizable_Array<T>::remove_value(T const &value) {
    for(s64 i = 0; i < this->count; ++i) {
        if(this->data[i] == value) {
            this->remove(i);
            break;
        }
    }
}

template<typename T>
void Resizable_Array<T>::remove_value_pointer(T *value) {
    for(s64 i = 0; i < this->count; ++i) {
        if(&this->data[i] == value) {
            this->remove(i);
            break;
        }
    }
}

template<typename T>
s64 Resizable_Array<T>::index_of(T const &value) {
    for(s64 i = 0; i < this->count; ++i) {
        if(this->data[i] == value) return i;
    }

    return -1;
}

template<typename T>
b8 Resizable_Array<T>::contains(T const &value) {
    for(s64 i = 0; i < this->count; ++i) {
        if(this->data[i] == value) return true;
    }

    return false;
}

template<typename T>
T *Resizable_Array<T>::push() {
    this->maybe_grow();
    T *pointer = &this->data[this->count];
    *pointer = T();
    ++this->count;
    return pointer;
}

template<typename T>
T Resizable_Array<T>::pop() {
    assert(this->count > 0);
    T value = this->data[this->count - 1];
    --this->count;
    this->maybe_shrink();
    return value;
}

template<typename T>
T Resizable_Array<T>::pop_first() {
    assert(this->count > 0);
    T value = this->data[0];
    this->remove(0);
    return value;
}

template<typename T>
Resizable_Array<T> Resizable_Array<T>::copy(Allocator *allocator) {
    Resizable_Array<T> result;
    result.allocator = allocator;
    result.reserve_exact(this->count);
    for(s64 i = 0; i < this->count; ++i) result.add(this->data[i]);
    return result;
}

template<typename T>
T &Resizable_Array<T>::operator[](s64 index) {
    assert(index >= 0 && index < this->count);
    return this->data[index];
}



/* ------------------------------------------ Resizable Block Array ------------------------------------------ */

template<typename T, s64 block_capacity>
s64 Resizable_Block_Array<T, block_capacity>::calculate_block_entry_count(Block *block) {
    if(block == this->last) return this->count - (this->block_count - 1) * block_capacity;
    return block_capacity;
}

template<typename T, s64 block_capacity>
typename Resizable_Block_Array<T, block_capacity>::Block *Resizable_Block_Array<T, block_capacity>::find_previous_to_block(Block *block) {
    if(block == this->first) return null;

    Block *previous = this->first;
    while(previous->next != block) {
        previous = previous->next;
    }

    return previous;
}

template<typename T, s64 block_capacity>
typename Resizable_Block_Array<T, block_capacity>::Block *Resizable_Block_Array<T, block_capacity>::find_block(s64 index, s64 *index_in_block) {
    s64 block_index = index / block_capacity;
    *index_in_block = index - block_index * block_capacity;

    Block *block = this->first;
    while(block_index > 0) {
        block = block->next;
        --block_index;
    }
    
    return block;
}

template<typename T, s64 block_capacity>
void Resizable_Block_Array<T, block_capacity>::move_entries(Block *src_block, s64 src_index_in_block, s64 dst_index_in_block) {
    if(dst_index_in_block > src_index_in_block) {
        if(src_block->next) this->move_entries(src_block->next, src_index_in_block, dst_index_in_block);

        s64 entries_in_block              = this->calculate_block_entry_count(src_block);
        s64 entries_moved_in_block        = dst_index_in_block - src_index_in_block;
        s64 entries_staying_in_this_block = block_capacity - dst_index_in_block;
        
        b8 needs_to_move_stuff_to_next_block = src_block->next != null || dst_index_in_block + entries_moved_in_block > block_capacity; // We can maybe save us moving stuff to the next block if we are the very last, and the moved data still fits into this block.
        if(needs_to_move_stuff_to_next_block) {
            if(src_block->next == null) this->maybe_grow(true);

            s64 entries_moving_to_next_block = entries_in_block - entries_staying_in_this_block;
            Block *dst_block = src_block->next;
            memmove(&dst_block->data[0], &src_block->data[entries_staying_in_this_block], entries_moving_to_next_block * sizeof(T));
        }
        
        memmove(&src_block->data[dst_index_in_block], &src_block->data[src_index_in_block], entries_staying_in_this_block * sizeof(T));
    } else if(dst_index_in_block < src_index_in_block) {
        s64 entries_coming_from_this_block = block_capacity - src_index_in_block;
        
        memmove(&src_block->data[dst_index_in_block], &src_block->data[src_index_in_block], entries_coming_from_this_block * sizeof(T));

        b8 needs_to_take_stuff_from_next_block = src_block->next != null;
        if(needs_to_take_stuff_from_next_block) {
            s64 entries_coming_from_next_block = src_index_in_block - dst_index_in_block;
            Block *dst_block = src_block->next;
            memmove(&src_block->data[dst_index_in_block + entries_coming_from_this_block], &dst_block->data[0], entries_coming_from_next_block * sizeof(T));
        }
        
        if(src_block->next) this->move_entries(src_block->next, src_index_in_block, dst_index_in_block);
    }
}

template<typename T, s64 block_capacity>
void Resizable_Block_Array<T, block_capacity>::maybe_grow(b8 force) {
    if(!this->first) {
        Block *block = (Block *) this->allocator->allocate(sizeof(Block));
        block->next  = null;

        this->first = this->last = block;
        ++this->block_count;
    } else if(force || this->count % block_capacity == 0) {
        Block *block = (Block *) this->allocator->allocate(sizeof(Block));
        block->next  = null;

        this->last->next = block;
        this->last = block;
        ++this->block_count;
    }
}

template<typename T, s64 block_capacity>
void Resizable_Block_Array<T, block_capacity>::maybe_shrink() {
    if(this->last == null) return;

    while(this->count - (this->block_count - 1) * block_capacity == 0) {
        Block *previous_to_last = this->find_previous_to_block(this->last);

        Block *block_to_free = this->last;
        this->allocator->deallocate(block_to_free);
        --this->block_count;

        previous_to_last->next = null;
        this->last = previous_to_last;
    }
}

template<typename T, s64 block_capacity>
void Resizable_Block_Array<T, block_capacity>::clear() {
    Block *block = this->first;
    while(block) {
        Block *next = block->next;
        this->allocator->deallocate(block);
        block = next;
    }

    this->count = 0;
    this->first = null;
    this->last  = null;
}

template<typename T, s64 block_capacity>
void Resizable_Block_Array<T, block_capacity>::clear_without_deallocation() {
    this->first = null;
    this->last  = null;
    this->count = 0;
}

template<typename T, s64 block_capacity>
void Resizable_Block_Array<T, block_capacity>::add(T const &data) {
    this->maybe_grow();

    s64 index_in_block;
    Block *block = this->find_block(this->count, &index_in_block);
    block->data[index_in_block] = data;

    ++this->count;
}

template<typename T, s64 block_capacity>
void Resizable_Block_Array<T, block_capacity>::insert(s64 index, T const &data) {
    assert(index >= 0 && index <= this->count);

    s64 index_in_block;
    Block *block = this->find_block(index, &index_in_block);

    this->move_entries(block, index_in_block, index_in_block + 1);

    block->data[index_in_block] = data;

    ++this->count;
}

template<typename T, s64 block_capacity>
void Resizable_Block_Array<T, block_capacity>::remove(s64 index) {
    assert(index >= 0 && index < this->count);

    s64 index_in_block;
    Block *block = this->find_block(index, &index_in_block);

    this->move_entries(block, index_in_block + 1, index_in_block);
    --this->count;

    this->maybe_shrink();
}

template<typename T, s64 block_capacity>
void Resizable_Block_Array<T, block_capacity>::remove_range(s64 first, s64 last) {
    // We don't know if first and last are in the same block, if they span over more than one block, etc.
    // That makes a general "optimized" solution (by removing the entire range at once) pretty difficult...
    for(s64 i = first; i <= last; ++i) this->remove(first);
}

template<typename T, s64 block_capacity>
void Resizable_Block_Array<T, block_capacity>::remove_value(T const &value) {
    for(s64 i = 0; i < this->count; ++i) {
        if(this->operator[](i) == value) {
            this->remove(i);
            break;
        }
    }
}

template<typename T, s64 block_capacity>
void Resizable_Block_Array<T, block_capacity>::remove_value_pointer(T *pointer) {
    for(s64 i = 0; i < this->count; ++i) {
        s64 index_in_block;
        Block *block = this->find_block(this->count, &index_in_block);
        if(&block->data[index_in_block] == pointer) {
            this->remove(i);
            break;
        }
    }
}

template<typename T, s64 block_capacity>
b8 Resizable_Block_Array<T, block_capacity>::contains(T const &value) {
    for(s64 i = 0; i < this->count; ++i) {
        s64 index_in_block;
        Block *block = this->find_block(this->count, &index_in_block);
        if(block->data[index_in_block] == value) {
            return true;
        }
    }

    return false;
}

template<typename T, s64 block_capacity>
T *Resizable_Block_Array<T, block_capacity>::push() {
    this->add(T());
    return &this->operator[](this->count - 1);
}

template<typename T, s64 block_capacity>
void Resizable_Block_Array<T, block_capacity>::pop() {
    assert(this->count > 0);
    this->remove(this->count - 1);
}

template<typename T, s64 block_capacity>
void Resizable_Block_Array<T, block_capacity>::pop_first() {
    assert(this->count > 0);
    this->remove(0);
}

template<typename T, s64 block_capacity>
Resizable_Block_Array<T, block_capacity> Resizable_Block_Array<T, block_capacity>::copy() {
    Resizable_Block_Array<T, block_capacity> copy{};
    copy.allocator = this->allocator;

    Block *my_block = this->first;
    while(my_block) {
        copy.maybe_grow(true);
        Block *other_block = copy.last;

        s64 count = this->calculate_block_entry_count(my_block);
        memcpy(other_block->data, my_block->data, count * sizeof(T));
        copy.count += count;
        
        my_block = my_block->next;
    }
    
    return copy;
}

template<typename T, s64 block_capacity>
T &Resizable_Block_Array<T, block_capacity>::operator[](s64 index) {
    assert(index >= 0 && index < this->count);
    s64 index_in_block;
    Block *block = this->find_block(index, &index_in_block);
    return block->data[index_in_block];
}



/* ----------------------------------------------- Linked List ----------------------------------------------- */

template<typename T>
Linked_List_Node<T> *Linked_List<T>::make_node(T const &value) {
    Linked_List_Node<T> *node = (Linked_List_Node<T> *) this->allocator->allocate(sizeof(Linked_List_Node<T>));
    node->next = null;
    node->data = value;
    return node;
}

template<typename T>
void Linked_List<T>::clear() {
    Linked_List_Node<T> *node = this->head;

    while(node) {
        auto next = node->next;
        this->allocator->deallocate(node);
        node = next;
    }

    this->head = null;
    this->tail = null;
    this->count = 0;
}

template<typename T>
void Linked_List<T>::add(T const &value) {
    Linked_List_Node<T> *node = this->make_node(value);
		
    if(this->head) {
        this->tail->next = node;
        this->tail = node;
    } else {
        this->head = node;
        this->tail = this->head;
    }

    ++this->count;
}

template<typename T>
void Linked_List<T>::add_first(T const &value) {
    Linked_List_Node<T> *node = this->make_node(value);
		
    if(this->head) {
        node->next = this->head;
        this->head = node;
    } else {
        this->head = node;
        this->tail = this->head;
    }

    ++this->count;
}

template<typename T>
void Linked_List<T>::remove_node(Linked_List_Node<T> *node) {
    if(!node) return;

    if(node != this->head) {
        Linked_List_Node<T> *previous = this->head;

        while(previous && previous->next != node) {
            previous = previous->next;
        }

        assert(previous != null);
        previous->next = node->next;

        if(this->tail == node) this->tail = previous;
    } else {
        this->head = this->head->next;
    }

    this->allocator->deallocate(node);

    
    --this->count;
}

template<typename T>
void Linked_List<T>::remove_value(T const &value) {
    Linked_List_Node<T> *node = this->head;

    while(node && node->data != value) {
        node = node->next;
    }

    this->remove_node(node);
}

template<typename T>
void Linked_List<T>::remove_value_pointer(T *value_pointer) {
    Linked_List_Node<T> *node = this->head;

    while(node && &node->data != value_pointer) {
        node = node->next;
    }

    this->remove_node(node);
}

template<typename T>
void Linked_List<T>::remove(s64 index) {
    assert(index >= 0 && index < this->count);
		
    Linked_List_Node<T> *node = this->head;

    while(index > 0) {
        node = node->next;
        --index;
    }

    this->remove_node(node);
}

template<typename T>
b8 Linked_List<T>::contains(T const &value) {
    Linked_List_Node<T> *node = this->head;

    while(node) {
        if(node->data == value) return true;
        node = node->next;
    }

    return false;
}

template<typename T>
T *Linked_List<T>::push() {
    this->add(T{});
    return &this->tail->data;
}


template<typename T>
T Linked_List<T>::pop() {
    assert(this->count > 0);
    T value = this->tail->data;
    this->remove_node(this->tail);
    return value;
}

template<typename T>
T Linked_List<T>::pop_first() {
    assert(this->count > 0);
    T value = this->head->value;
    this->remove_node(this->head);
    return value;
}

template<typename T>
T &Linked_List<T>::operator[](s64 index) {
    assert(index >= 0 && index < this->count);

    Linked_List_Node<T> *node = this->head;

    while(index > 0) {
        node = node->next;
        --index;
    }

    return node->data;
}
