template<typename T>
void Data_Array<T>::create(Allocator *allocator, s64 capacity) {
    this->allocator = allocator;
    this->capacity  = capacity;
    this->count     = 0;

    this->indirection = (s64 *) this->allocator->allocate(this->capacity * sizeof(s64));
    this->id          = (Data_Array_Id *) this->allocator->allocate(this->capacity * sizeof(Data_Array_Id));
    this->data        = (T *) this->allocator->allocate(this->capacity * sizeof(T));

    this->free_list_count = 0;
    this->free_list = null;

    for(s64 i = 0; i < this->capacity; ++i) this->indirection[i] = INVALID_DATA_ARRAY_INDEX;
}

template<typename T>
void Data_Array<T>::destroy() {
    this->count              = 0;
    this->capacity           = 0;
    this->free_list_count    = 0;
    this->free_list_capacity = 0;
    this->allocator->deallocate(this->id);
    this->allocator->deallocate(this->data);
    this->allocator->deallocate(this->indirection);
    this->allocator->deallocate(this->free_list);

    this->id          = null;
    this->data        = null;
    this->indirection = null;
    this->free_list   = null;
}

template<typename T>
Data_Array_Id Data_Array<T>::push() {
    assert(this->count < this->capacity, "Data Array reached its capacity.");

    Data_Array_Id id;
    
    if(this->free_list_count) {
        id = this->pop_free_list();
    } else {
        // This means that all ids between 0 and the highest id are used. Since we only have 'count' active
        // ids, it means that all active ids are 0 to count.
        id = (Data_Array_Id) this->count;
        assert(!this->id_is_valid(id), "This id was expected to be unused.");
    }

    this->indirection[id]   = this->count;
    this->id[this->count]   = id;
    this->data[this->count] = T();

    ++this->count;

    return id;
}

template<typename T>
T *Data_Array<T>::push_with_id(Data_Array_Id id) {
    assert(this->count < this->capacity, "Data Array reached its capacity.");
    assert(!this->id_is_valid(id), "The supplied Data Array Id is already in use.");

    this->indirection[id]   = this->count;
    this->id[this->count]   = id;
    this->data[this->count] = T();
    T *ptr = &this->data[this->count];
    
    ++this->count;

    return ptr;
}

template<typename T>
void Data_Array<T>::remove_by_id(Data_Array_Id id) {
    assert(this->id_is_valid(id), "Tried to remove an invalid Data Array Id.");
    s64 index = this->indirection[id];
    this->remove_by_index(index);
}

template<typename T>
void Data_Array<T>::remove_by_index(s64 removed_index) {
    assert(index >= 0 && index < this->count, "Tried to remove an invalid Data Array Index.");

    s64 last_index = this->count - 1;
    
    Data_Array_Id last_id    = this->id[last_index];
    Data_Array_Id removed_id = this->id[removed_index];

    this->data[removed_index] = this->data[last_index];

    this->id[removed_index] = last_id;
    this->id[last_index] = INVALID_DATA_ARRAY_ID;

    this->indirection[removed_id] = INVALID_DATA_ARRAY_INDEX;
    this->indirection[last_id]    = removed_index;

    this->push_free_list(removed_id);
    
    --this->count;
}

template<typename T>
T *Data_Array<T>::index(s64 index) {
    assert(index >= 0 && index < this->count, "Data Array Index is out of bounds.");
    return &this->data[index];
}

template<typename T>
T *Data_Array<T>::query(Data_Array_Id id) {
    assert(this->id_is_valid(id), "Tried to query an invalid Data Array Id.");
    s64 index = this->indirection[id];
    return &this->data[index];
}

template<typename T>
b8 Data_Array<T>::id_is_valid(Data_Array_Id id) {
    if(id < 0 || id >= this->capacity) return false;
    return this->indirection[id] != INVALID_DATA_ARRAY_INDEX;
}

template<typename T>
void Data_Array<T>::clear_free_list() {
    this->allocator->deallocate(this->free_list);
    this->free_list          = null;
    this->free_list_count    = 0;
    this->free_list_capacity = 0;
}

template<typename T>
void Data_Array<T>::push_free_list(Data_Array_Id id) {
    if(this->free_list_capacity == 0) {
        this->free_list_capacity = 8;
        this->free_list = this->allocator->allocate(this->free_list_capacity * sizeof(Data_Array_Id));
    }

    if(this->free_list_count == this->free_list_capacity) {
        this->free_list_capacity *= 2;
        this->free_list = this->allocator->reallocate(this->free_list, this->free_list_capacity * sizeof(Data_Array_Id));
    }
    
    this->free_list[this->free_list_count] = id;
    ++this->free_list_count;
}

template<typename T>
Data_Array_Id Data_Array<T>::pop_free_list() {
    assert(this->free_list_count > 0);
    Data_Array_Id id = this->free_list[0];

    memmove(&this->free_list[0], &this->free_list[1], (this->free_list_count - 1) * sizeof(Data_Array_Id));
    --this->free_list_count;

    if(this->free_list_count == 0) {
        this->clear_free_list();
    } else if(this->free_list_count < this->free_list_capacity / 3 && this->free_list_capacity >= 8) {
        this->free_list_capacity /= 2;
        assert(this->free_list_count <= this->free_list_capacity);
        this->free_list = (Data_Array_Id *) this->allocator->reallocate(this->free_list, this->free_list_capacity * sizeof(Data_Array_Id));
    }
    
    return id;
}

template<typename T>
void Data_Array<T>::rebuild_free_list() {
    //
    // When deserializing a data array from disk the IDs are often restored (to guarantee ID consistency betweeen
    // saves / loads). When doing this, potential gaps in the used ID's may be opened (e.g. if an entity was
    // deleted), and this gap must be represented in the free-list.
    // If we don't do this, then array.count would be smaller than the highest used ID, which will lead to
    // duplicate IDs when pushing the data array.
    //
    // In short: The free list must contain all unused IDs between zero and the highest active ID.
    //
    this->clear_free_list();

    Data_Array_Id highest_used_id = 0;

    for(s64 i = 0; i < this->allocated; ++i) {
        if(this->indirection[i] != INVALID_DATA_ARRAY_INDEX) highest_used_id = i;
    }
        
    for(s64 i = 0; i < this->allocated; ++i) {
        if(this->indirection[i] == INVALID_DATA_ARRAY_INDEX) this->add_to_free_list(i);
    }
}
