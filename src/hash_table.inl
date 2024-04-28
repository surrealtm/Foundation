//
// Careful:
// This source file gets #include'd in the header file, because templates are shit!
//
#include <intrin.h>

static inline
s64 __hash_table_next_power_of_two(s64 size) {
#if FOUNDATION_WIN32
    if(size & (size - 1)) {
        return 1ull << (64 - __lzcnt64(size)); // Round up to next power of two.
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
    this->buckets         = (Entry *) this->allocator->allocate(this->bucket_count * sizeof(Entry));
    this->bucket_occupied = (b8 *) this->allocator->allocate(this->bucket_count * sizeof(b8));
    memset(this->bucket_occupied, 0, this->bucket_count * sizeof(b8));

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
    Entry *previous_buckets      = this->buckets;
    b8 *previous_bucket_occupied = this->bucket_occupied;
    s64 previous_bucket_count    = this->bucket_count;
    
    //
    // Allocate the new hash table.
    //
    this->count           = 0;
    this->bucket_count    = __hash_table_next_power_of_two(bucket_count);
    this->bucket_mask     = this->bucket_count - 1; // Mask all lower bits so that we can map hash values to the bucket count.
    this->buckets         = (Entry *) this->allocator->allocate(this->bucket_count * sizeof(Entry));
    this->bucket_occupied = (b8 *) this->allocator->allocate(this->bucket_count * sizeof(b8));
    memset(this->bucket_occupied, 0, this->bucket_count * sizeof(b8));

#if FOUNDATION_DEVELOPER
    this->stats = Hash_Table_Stats();
#endif
    
    //
    // Copy all hash table entries into the new one.
    //
    for(s64 i = 0; i < previous_bucket_count; ++i) {
        if(!previous_bucket_occupied[i]) continue;
        
        auto *bucket = &previous_buckets[i];
        while(bucket) {
            this->add(bucket->key, bucket->value);
            if(bucket != &previous_buckets[i]) {
                auto *next = bucket->next;
                this->allocator->deallocate(bucket);
                bucket = next;
            } else {
                bucket = bucket->next;
            }
        }
    }

    //
    // Deallocate the old hash table.
    //
    this->allocator->deallocate(previous_buckets);
    this->allocator->deallocate(previous_bucket_occupied);
}

template<typename K, typename V>
void Chained_Hash_Table<K, V>::destroy() {
    for(s64 i = 0; i < this->bucket_count; ++i) {
        auto *bucket = this->buckets[i].next;

        while(bucket) {
            auto *next = bucket->next;
            this->allocator->deallocate(bucket);
            bucket = next;
        }
    }

    this->allocator->deallocate(this->buckets);
    this->allocator->deallocate(this->bucket_occupied);
    this->buckets         = null;
    this->bucket_occupied = null;
    this->bucket_mask     = 0;
    this->bucket_count    = 0;
    this->count           = 0;
}

template<typename K, typename V>
void Chained_Hash_Table<K, V>::add(K const &k, V const &v) {
    u64 bucket_index = this->find_bucket_index(k);
    
    auto *existing_entry = this->find_entry(k, bucket_index);
    if(existing_entry) {
        existing_entry->value = v;
        return;
    }

    assert(bucket_index >= 0 && (s64) bucket_index < this->bucket_count);

    if(this->bucket_occupied[bucket_index]) {
        Entry *entry = (Entry *) this->allocator->allocate(sizeof(Entry));
        entry->next  = this->buckets[bucket_index].next;
        entry->key   = k;
        entry->value = v;
        this->buckets[bucket_index].next = entry;

#if FOUNDATION_DEVELOPER
        this->stats.collisions += 1;
#endif
    } else {
        this->buckets[bucket_index].next    = null;
        this->buckets[bucket_index].key     = k;
        this->buckets[bucket_index].value   = v;
        this->bucket_occupied[bucket_index] = true;
    }
    
    ++this->count;

#if FOUNDATION_DEVELOPER
    this->stats.load_factor = (f32) this->count / (f32) this->bucket_count;
#endif
}

template<typename K, typename V>
void Chained_Hash_Table<K, V>::remove(K const &k) {   
    u64 bucket_index = this->find_bucket_index(k);

    if(!this->bucket_occupied[bucket_index]) return;

    Entry *previous = null;
    Entry *entry = &this->buckets[bucket_index];
    while(entry && !this->compare(entry->key, k)) {
        previous = entry;
        entry = entry->next;
    }

    if(!entry) return;

    if(previous) {
        previous->next = entry->next;
        this->allocator->deallocate(entry);
    } else if(entry->next) {
        auto *next = entry->next;
        *entry = *entry->next;
        this->allocator->deallocate(next);
    } else {
        this->bucket_occupied[bucket_index] = false;
    }

    --this->count;

#if FOUNDATION_DEVELOPER
    this->stats.load_factor = (f32) this->count / (f32) this->bucket_count;
#endif
}

template<typename K, typename V>
V *Chained_Hash_Table<K, V>::query(K const &k) {
    u64 bucket_index = this->find_bucket_index(k);
    Entry *entry = this->find_entry(k, bucket_index);
    return entry ? &entry->value : null;
}

template<typename K, typename V>
u64 Chained_Hash_Table<K, V>::find_bucket_index(K const &k) {
    return this->hash(k) & this->bucket_mask;
}

template<typename K, typename V>
typename Chained_Hash_Table<K, V>::Entry *Chained_Hash_Table<K, V>::find_entry(K const &k, u64 bucket_index) {
    assert(bucket_index >= 0 && (s64) bucket_index < this->bucket_count);

    if(!this->bucket_occupied[bucket_index]) return null;

    auto *entry = &this->buckets[bucket_index];
    while(entry && !this->compare(entry->key, k)) entry = entry->next;

    return entry;
}

template<typename K, typename V>
f64 Chained_Hash_Table<K, V>::expected_number_of_collisions() {
    // https://blogs.asarkar.com/assets/docs/algorithms-curated/Probability%20Calculations%20in%20Hashing.pdf
    return (f64) this->count - (f64) this->bucket_count + (f64) this->bucket_count * pow(1.0 - (1.0 / (f64) this->bucket_count), (f64) this->count);
}



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
    if(this->count == this->bucket_count) return;

    u64 slot = this->hash(k) & this->bucket_mask;

    while(this->buckets[slot].state == HASH_TABLE_ENTRY_Used) {
#if FOUNDATION_DEVELOPER
        this->stats.collisions += 1;
#endif

        slot = (slot + 1) & this->bucket_mask;
    }

    this->buckets[slot].state = HASH_TABLE_ENTRY_Used;
    this->buckets[slot].key   = k;
    this->buckets[slot].value = v;

    ++this->count;
    
#if FOUNDATION_DEVELOPER
    this->stats.load_factor = (f64) this->count / (f64) this->bucket_count;
#endif
}

template<typename K, typename V>
void Probed_Hash_Table<K, V>::remove(K const &k) {
    u64 preferred_slot = this->hash(k) & this->bucket_mask;
    u64 current_slot = preferred_slot;

    while(this->buckets[current_slot].state != HASH_TABLE_ENTRY_Used || !this->compare(this->buckets[current_slot].key, k)) {
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
V *Probed_Hash_Table<K, V>::query(K const &k) {
    u64 preferred_slot = this->hash(k) & this->bucket_mask;
    u64 current_slot = preferred_slot;

    while(this->buckets[current_slot].state != HASH_TABLE_ENTRY_Used || !this->compare(this->buckets[current_slot].key, k)) {
        current_slot = (current_slot + 1) & this->bucket_mask;
        
        if(current_slot == preferred_slot) return null;
    }    

    return &this->buckets[current_slot].value;
}

template<typename K, typename V>
f64 Probed_Hash_Table<K, V>::expected_number_of_collisions() {
    // https://blogs.asarkar.com/assets/docs/algorithms-curated/Probability%20Calculations%20in%20Hashing.pdf
    return (f64) this->count - (f64) this->bucket_count + (f64) this->bucket_count * pow(1.0 - (1.0 / (f64) this->bucket_count), (f64) this->count);
}



/* ---------------------------------------- Predefined Hash Functions ---------------------------------------- */

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
