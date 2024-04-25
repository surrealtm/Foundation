//
// Careful:
// This source file gets #include'd in the header file, because templates are shit!
//

template<typename K, typename V>
void Hash_Table<K, V>::create(s64 bucket_count, Hash_Table_Hash_Procedure hash, Hash_Table_Comparison_Procedure compare) {
    if(bucket_count <= 0) return;

#if FOUNDATION_WIN32
    u64 highest_bit_set;
    _BitScanForward64(&highest_bit_set, bucket_count);
    this->bucket_count    = 1ull << (64 - highest_bit_set); // Round bucket count up to be the next power of two.
#endif

    this->count           = 0;
    this->mask            = this->bucket_count - 1; // Mask all lower bits so that we can map hash values to the bucket count.
    this->buckets         = (Hash_Table_Entry<K, V> *) this->allocator->allocate(this->bucket_count * sizeof(Hash_Table_Entry<K, V>));
    this->bucket_occupied = (b8 *) this->allocator->allocate(this->bucket_count * sizeof(b8));
    this->hash            = hash;
    this->compare         = compare;
    memset(this->bucket_occupied, 0, this->bucket_count * sizeof(b8));
}

template<typename K, typename V>
void Hash_Table<K, V>::destroy() {
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
void Hash_Table<K, V>::add(K const &k, V const &v) {
    u64 bucket_index = this->find_bucket_index(k);
    
    auto *existing_entry = this->find_entry(k, bucket_index);
    if(existing_entry) {
        existing_entry->value = v;
        return;
    }

    assert(bucket_index >= 0 && bucket_index < this->bucket_count);

    if(this->bucket_occupied[bucket_index]) {
        Hash_Table_Entry<K, V> *entry = (Hash_Table_Entry<K, V> *) this->allocator->allocate(sizeof(Hash_Table_Entry<K, V>));
        entry->next  = this->buckets[bucket_index].next;
        entry->key   = k;
        entry->value = v;
        this->buckets[bucket_index].next = entry;
    } else {
        this->buckets[bucket_index].next     = null;
        this->buckets[bucket_index].key      = k;
        this->buckets[bucket_index].value    = v;
        this->bucket_occupied[bucket_index] = true;
    }

    ++this->count;
}

template<typename K, typename V>
void Hash_Table<K, V>::remove(K const &k) {   
    u64 bucket_index = this->find_bucket_index(k);

    if(!this->bucket_occupied[bucket_index]) return;

    Hash_Table_Entry *previous = null;
    auto *entry = &this->buckets[bucket_index];
    while(entry && !this->compare(entry->k, k)) {
        previous = entry;
        entry = entry->next;
    }

    if(!entry) return;

    if(previous) {
        previous->next = entry->next;
        this->allocator->deallocate(entry);
    } else if(entry->next) {
        *entry = *entry->next;
    } else {
        this->buckets_occupied[bucket_index] = false;
    }

    --count;
}

template<typename K, typename V>
V *Hash_Table<K, V>::query(K const &k) {
    u64 bucket_index = this->find_bucket_index(k);
    auto *entry = this->find_entry(k, bucket_index);

    return entry ? &entry->value : null;
}

template<typename K, typename V>
u64 Hash_Table<K, V>::find_bucket_index(K const &k) {
    return this->hash(k) & this->bucket_mask;
}

template<typename K, typename V>
Hash_Table_Entry<K, V> *Hash_Table<K, V>::find_entry(K const &k, u64 bucket_index) {
    assert(bucket_index >= 0 && bucket_index < this->bucket_count);

    if(!this->bucket_occupied[bucket_index]) return null;

    auto *entry = &this->buckets[bucket_index];
    while(entry && !this->compare(entry->key, k)) entry = entry->next;

    return entry;
}
