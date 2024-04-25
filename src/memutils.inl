//
// Careful:
// This source file gets #include'd in the header file, because templates are shit!
//

template<typename T>
void Resizable_Array<T>::maybe_grow(b8 force) {
    if(!this->data) {
        this->allocated = Resizable_Array::INITIAL_SIZE;
        this->data      = (T *) this->allocator->allocate(this->allocated * sizeof(T));
    } else if(force || this->count == this->allocated) {
        this->allocated *= 2;
		
        if(!this->allocator->_reallocate_procedure) {
            // Not all allocators actually provide a reallocation strategy (e.g. Memory Arenas). In that case,
            // allocate new memory manually, copy the existing data and if the allocator does have a
            // deallocation strategy, free the previous pointer. If the allocator does not have a
            // deallocation, then it is most likely some sort of scratch allocator that frees all memory at
            // once.
            T *new_pointer = (T *) this->allocator->allocate(this->allocated * sizeof(T));
            memcpy(new_pointer, this->data, this->count * sizeof(T));
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

        if(!this->allocator->_reallocate_procedure) {
            // Not all allocators actually provide a reallocation strategy (e.g. Memory Arenas). In that case,
            // allocate new memory manually, copy the existing data and if the allocator does have a
            // deallocation strategy, free the previous pointer. If the allocator does not have a
            // deallocation, then it is most likely some sort of scratch allocator that frees all memory at
            // once.
            T *new_pointer = (T *) this->allocator->allocate(this->allocated * sizeof(T));
            memcpy(new_pointer, this->data, this->count * sizeof(T));
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
    this->count = 0;
    this->allocated = 0;
    this->data = null;
}

template<typename T>
void Resizable_Array<T>::reserve(s64 count) {
    assert(count >= 0);
    if(this->allocated == 0) this->allocated = Resizable_Array::INITIAL_SIZE;
    s64 least_size = this->allocated + count;
    while(this->allocated < least_size) this->allocated *= 2;
    this->maybe_grow(true);    
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
    if(index < this->count) memcpy(&this->data[index + 1], &this->data[index], (this->count - index) * sizeof(T));
    this->data[index] = data;
    ++this->count;
}

template<typename T>
void Resizable_Array<T>::remove(s64 index) {
    assert(index >= 0 && index < this->count);
    memcpy(&this->data[index], &this->data[index + 1], (this->count - index) * sizeof(T));
    --this->count;
    this->maybe_shrink();
}

template<typename T>
void Resizable_Array<T>::remove_range(s64 first_to_remove, s64 last_to_remove) {
    assert(first_to_remove >= 0 && first_to_remove < this->count);
    assert(last_to_remove >= 0 && last_to_remove < this->count);
    memcpy(&this->data[first_to_remove], &this->data[last_to_remove + 1], (this->count - last_to_remove - 1) * sizeof(T));
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
T &Resizable_Array<T>::operator[](s64 index) {
    assert(index >= 0 && index < this->count);
    return this->data[index];
}



template<typename T>
Linked_List_Node<T> *Linked_List<T>::make_node(T const &value) {
    Linked_List_Node<T> *node = (Linked_List_Node<T> *) this->allocator->allocate(sizeof(Linked_List_Node<T>));
    node->next = null;
    node->data = value;
    return node;
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

    while(node && node->value != value) {
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
T *Linked_List<T>::push() {
    this->add(T{});
    return &this->tail->data;
}

template<typename T>
T Linked_List<T>::pop() {
    assert(this->count > 0);
    T value = this->tail->value;
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
