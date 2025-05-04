//
// Careful:
// This source file gets #include'd in the header file, because templates are shit!
//

#if FOUNDATION_WIN32
extern "C" {
    unsigned __int64 __lzcnt64(unsigned __int64); // From intrin.h
};
#endif

static inline
s64 __hash_table_next_power_of_two(s64 size) {
#if FOUNDATION_WIN32
    if(size & (size - 1)) {
        return 1ull << (64 - __lzcnt64(size)); // Round up to next power of two.
    }

    return size; // Already is a power of two!
#endif

#if FOUNDATION_LINUX
    if(size & (size - 1)) {
        return 1ull << (64 - __builtin_clzl(size)); // Round up to next power of two.
    }

    return size; // Already is a power of two!
#endif
}


/* -------------------------------------------- Chained Hash Table -------------------------------------------- */

template<typename K, typename V>
void Chained_Hash_Table<K, V>::create(s64 bucket_count, Hash_Procedure hash, Comparison_Procedure compare) {
    this->hash    = hash;
    this->compare = compare;
    this->count   = 0;

    if(bucket_count <= 0) return;

    this->bucket_count    = __hash_table_next_power_of_two(bucket_count);
    this->bucket_mask     = this->bucket_count - 1; // Mask all lower bits so that we can map hash values to the bucket count.
    this->buckets         = (Entry **) this->allocator->allocate(this->bucket_count * sizeof(Entry *));

#if FOUNDATION_DEVELOPER
    this->stats = Hash_Table_Stats();
#endif
}

template<typename K, typename V>
void Chained_Hash_Table<K, V>::resize(s64 bucket_count) {
    if(bucket_count <= 0) return;

    //
    // Temporarily save the old hash table.
    //
    Entry **previous_buckets  = this->buckets;
    s64 previous_bucket_count = this->bucket_count;
    
    //
    // Allocate the new hash table.
    //
    this->count        = 0;
    this->bucket_count = __hash_table_next_power_of_two(bucket_count);
    this->bucket_mask  = this->bucket_count - 1; // Mask all lower bits so that we can map hash values to the bucket count.
    this->buckets      = (Entry *) this->allocator->allocate(this->bucket_count * sizeof(Entry));

#if FOUNDATION_DEVELOPER
    this->stats = Hash_Table_Stats();
#endif
    
    //
    // Copy all hash table entries into the new one.
    //
    for(s64 i = 0; i < previous_bucket_count; ++i) {       
        auto *bucket = previous_buckets[i];
        while(bucket) {
            this->add(bucket->key, bucket->value);
            auto *next = bucket->next;
            this->allocator->deallocate(bucket);
            bucket = next;
        }
    }

    //
    // Deallocate the old hash table.
    //
    this->allocator->deallocate(previous_buckets);
}

template<typename K, typename V>
void Chained_Hash_Table<K, V>::destroy() {
    for(s64 i = 0; i < this->bucket_count; ++i) {
        auto *bucket = this->buckets[i];

        while(bucket) {
            auto *next = bucket->next;
            this->allocator->deallocate(bucket);
            bucket = next;
        }
    }

    this->allocator->deallocate(this->buckets);
    this->buckets         = null;
    this->bucket_mask     = 0;
    this->bucket_count    = 0;
    this->count           = 0;
}

template<typename K, typename V>
void Chained_Hash_Table<K, V>::add(K const &k, V const &v) {
    V *value = this->push(k);
    *value = v;
}

template<typename K, typename V>
void Chained_Hash_Table<K, V>::remove(K const &k) {   
    u64 hash = this->hash(k);
    u64 bucket_index = hash & this->bucket_mask;

    Entry *previous = null;
    Entry *entry = this->buckets[bucket_index];
    while(entry && (entry->hash != hash || !this->compare(entry->key, k))) {
        previous = entry;
        entry = entry->next;
    }

    if(!entry) return;

    if(previous) {
        previous->next = entry->next;
    } else {
        this->buckets[bucket_index] = entry->next;
    }

    this->allocator->deallocate(entry);
    --this->count;

#if FOUNDATION_DEVELOPER
    this->stats.load_factor = (f32) this->count / (f32) this->bucket_count;
#endif
}

template<typename K, typename V>
V *Chained_Hash_Table<K, V>::push(K const &k) {
    assert(!this->query(k));

    u64 hash = this->hash(k);
    u64 bucket_index = hash & this->bucket_mask;

    Entry *entry = (Entry *) this->allocator->allocate(sizeof(Entry));

    if(this->buckets[bucket_index]) {
        entry->next  = this->buckets[bucket_index];
        entry->hash  = hash;
        entry->key   = k;

#if FOUNDATION_DEVELOPER
        this->stats.collisions += 1;
#endif
    } else {
        entry->next    = null;
        entry->hash    = hash;
        entry->key     = k;
    }
    
    this->buckets[bucket_index] = entry;
    ++this->count;

#if FOUNDATION_DEVELOPER
    this->stats.load_factor = (f32) this->count / (f32) this->bucket_count;
#endif

    return &entry->value;
}

template<typename K, typename V>
V *Chained_Hash_Table<K, V>::query(K const &k) {
    u64 hash = this->hash(k);
    u64 bucket_index = hash & this->bucket_mask;
    
    auto *entry = this->buckets[bucket_index];
    while(entry && (entry->hash != hash || !this->compare(entry->key, k))) entry = entry->next;
    
    return entry ? &entry->value : null;
}

template<typename K, typename V>
typename Chained_Hash_Table<K, V>::Iterator Chained_Hash_Table<K, V>::begin() {
    Iterator iterator;

    iterator.table = this;
    iterator.bucket_index  = 0;

    while(iterator.bucket_index < this->bucket_count && this->buckets[iterator.bucket_index] == null) {
        ++iterator.bucket_index;
    }

    if(iterator.bucket_index < this->bucket_count) {
        iterator.entry_pointer = this->buckets[iterator.bucket_index];
    } else {
        iterator.entry_pointer = null;
    }
    
    return iterator;
}

template<typename K, typename V>
typename Chained_Hash_Table<K, V>::Iterator Chained_Hash_Table<K, V>::end() {
    Iterator iterator;
    iterator.table         = this;
    iterator.entry_pointer = null;
    iterator.bucket_index  = this->bucket_count;
    return iterator;
}

template<typename K, typename V>
f64 Chained_Hash_Table<K, V>::fill_factor() {
    return (f64) this->count / (f64) this->bucket_count;
}

#if FOUNDATION_DEVELOPER
template<typename K, typename V>
f64 Chained_Hash_Table<K, V>::expected_number_of_collisions() {
    // https://blogs.asarkar.com/assets/docs/algorithms-curated/Probability%20Calculations%20in%20Hashing.pdf
    return (f64) this->count - (f64) this->bucket_count + (f64) this->bucket_count * pow(1.0 - (1.0 / (f64) this->bucket_count), (f64) this->count);
}
#endif



/* -------------------------------------------- Probed Hash Table -------------------------------------------- */

template<typename K, typename V>
void Probed_Hash_Table<K, V>::create(s64 bucket_count, Hash_Procedure hash, Comparison_Procedure compare) {
    this->hash    = hash;
    this->compare = compare;
    this->count   = 0;

    if(bucket_count <= 0) return;
    
    this->bucket_count = __hash_table_next_power_of_two(bucket_count);
    this->bucket_mask  = this->bucket_count - 1; // Mask all lower bits so that we can map hash values to the bucket count.
    this->buckets      = (Entry *) this->allocator->allocate(this->bucket_count * sizeof(Entry));
    memset(this->buckets, 0, this->bucket_count * sizeof(Entry));

#if FOUNDATION_DEVELOPER
    this->stats = Hash_Table_Stats();
#endif
}

template<typename K, typename V>
void Probed_Hash_Table<K, V>::resize(s64 bucket_count) {
    if(bucket_count <= 0) return;

    //
    // Temporarily save the old hash table.
    //
    Entry *previous_buckets = this->buckets;
    s64 previous_bucket_count = this->bucket_count;

    //
    // Allocate the new hash table.
    //
    this->count        = 0;
    this->bucket_count = __hash_table_next_power_of_two(bucket_count);
    this->bucket_mask  = this->bucket_count - 1; // Mask all lower bits so that we can map hash values to the bucket count.
    this->buckets      = (Entry *) this->allocator->allocate(this->bucket_count * sizeof(Entry));
    memset(this->buckets, 0, this->bucket_count * sizeof(Entry));
    
#if FOUNDATION_DEVELOPER
    this->stats = Hash_Table_Stats();
#endif    

    //
    // Copy all hash table entries into the new one.
    //
    for(s64 i = 0; i < previous_bucket_count; ++i) {
        if(previous_buckets[i].state == HASH_TABLE_ENTRY_Used) {
            this->add(previous_buckets[i].key, previous_buckets[i].value);
        }
    }

    //
    // Deallocate the old hash table.
    //
    this->allocator->deallocate(previous_buckets);
}

template<typename K, typename V>
void Probed_Hash_Table<K, V>::destroy() {
    this->allocator->deallocate(this->buckets);
    this->buckets      = null;
    this->bucket_count = 0;
    this->bucket_mask  = 0;
    this->count        = 0;
}

template<typename K, typename V>
void Probed_Hash_Table<K, V>::add(K const &k, V const &v) {
    V *value = this->push(k);
    if(!value) return; // Hash table is full!

    *value = v;
}

template<typename K, typename V>
void Probed_Hash_Table<K, V>::remove(K const &k) {
    u64 hash           = this->hash(k);
    u64 preferred_slot = hash & this->bucket_mask;
    u64 current_slot   = preferred_slot;

    while(this->buckets[current_slot].state != HASH_TABLE_ENTRY_Used || this->buckets[current_slot].hash != hash || !this->compare(this->buckets[current_slot].key, k)) {
        current_slot = (current_slot + 1) & this->bucket_mask;
        
        if(current_slot == preferred_slot) return;
    }    

    this->buckets[current_slot].state = HASH_TABLE_ENTRY_Deleted;
    --this->count;
    
#if FOUNDATION_DEVELOPER
    this->stats.load_factor = (f64) this->count / (f64) this->bucket_count;
#endif
}

template<typename K, typename V>
V *Probed_Hash_Table<K, V>::push(K const &k) {
    if(this->count == this->bucket_count) return null;

    assert(!this->query(k));
    
    u64 hash = this->hash(k);
    u64 slot = hash & this->bucket_mask;

    while(this->buckets[slot].state == HASH_TABLE_ENTRY_Used) {
#if FOUNDATION_DEVELOPER
        this->stats.collisions += 1;
#endif

        slot = (slot + 1) & this->bucket_mask;
    }

    this->buckets[slot].state = HASH_TABLE_ENTRY_Used;
    this->buckets[slot].hash  = hash;
    this->buckets[slot].key   = k;

    ++this->count;
    
#if FOUNDATION_DEVELOPER
    this->stats.load_factor = (f64) this->count / (f64) this->bucket_count;
#endif

    return &this->buckets[slot].value;
}

template<typename K, typename V>
V *Probed_Hash_Table<K, V>::query(K const &k) {
    u64 hash           = this->hash(k);
    u64 preferred_slot = hash & this->bucket_mask;
    u64 current_slot   = preferred_slot;

    while(this->buckets[current_slot].state != HASH_TABLE_ENTRY_Free && (this->buckets[current_slot].hash != hash || !this->compare(this->buckets[current_slot].key, k))) {
        current_slot = (current_slot + 1) & this->bucket_mask;
        
        if(current_slot == preferred_slot) return null;
    }
    
    if(this->buckets[current_slot].state == HASH_TABLE_ENTRY_Free) return null;

    return &this->buckets[current_slot].value;
}

template<typename K, typename V>
typename Probed_Hash_Table<K, V>::Iterator Probed_Hash_Table<K, V>::begin() {
    Iterator iterator;
    iterator.table = this;
    iterator.bucket_index = 0;

    while(iterator.bucket_index < this->bucket_count && this->buckets[iterator.bucket_index].state != HASH_TABLE_ENTRY_Used) {
        ++iterator.bucket_index;
    }

    if(iterator.bucket_index < this->bucket_count) {
        iterator.bucket_pointer = &this->buckets[iterator.bucket_index];
    } else {
        iterator.bucket_pointer = null;
    }
    
    return iterator;
}

template<typename K, typename V>
typename Probed_Hash_Table<K, V>::Iterator Probed_Hash_Table<K, V>::end() {
    Iterator iterator;
    iterator.table         = this;
    iterator.bucket_index  = this->bucket_count;
    iterator.bucket_pointer = null;
    return iterator;
}

template<typename K, typename V>
f64 Probed_Hash_Table<K, V>::fill_factor() {
    return (f64) this->count / (f64) this->bucket_count;
}

#if FOUNDATION_DEVELOPER
template<typename K, typename V>
f64 Probed_Hash_Table<K, V>::expected_number_of_collisions() {
    // https://blogs.asarkar.com/assets/docs/algorithms-curated/Probability%20Calculations%20in%20Hashing.pdf
    return (f64) this->count - (f64) this->bucket_count + (f64) this->bucket_count * pow(1.0 - (1.0 / (f64) this->bucket_count), (f64) this->count);
}
#endif



/* ---------------------------------------- Predefined Hash Functions ---------------------------------------- */

static inline
u64 fnv1a_64(const void *data, u64 size) {
    u64 prime = 1099511628211; 
    u64 offset = 14695981039346656037U;
    
    u64 hash = offset;
    
    for(u64 i = 0; i < size; ++i) {
        hash ^= ((u8 *) data)[i];
        hash *= prime;
    }
    
    return hash;
}

static inline
u64 murmur_64a(u64 k) {
    u64 m = 0xc6a4a7935bd1e995;
    s32 r = 47;
    u64 h = 0x8445d61a4e774912 ^ (8 * m);
    k *= m;
    k ^= k >> r;
    k *= m;
    h ^= k;
    h *= m;
    h ^= h >> r;
    h *= m;
    h ^= h >> r;
    return h;
}
