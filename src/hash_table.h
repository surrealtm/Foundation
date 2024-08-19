#pragma once

#include "foundation.h"
#include "memutils.h"

#if FOUNDATION_DEVELOPER
struct Hash_Table_Stats {
    s64 collisions;
    f64 load_factor;
};
#endif

template<typename K, typename V>
struct Chained_Hash_Table {
    typedef u64(*Hash_Procedure)(K const &k);
    typedef b8(*Comparison_Procedure)(K const &lhs, K const &rhs);

    struct Entry {
        Entry *next;
        u64 hash;
        K key;
        V value;
    };

    struct Pair {
        u64 hash;
        K *key;
        V *value;
        Pair(u64 hash, K *key, V *value) : hash(hash), key(key), value(value) {};
    };

    struct Iterator {
        Chained_Hash_Table *table;
        Entry *entry_pointer;
        s64 bucket_index;
        
        b8 operator==(Iterator const &it) const { return this->entry_pointer == it.entry_pointer; }
        b8 operator!=(Iterator const &it) const { return this->entry_pointer != it.entry_pointer; }

        Iterator &operator++() {
            if(this->entry_pointer->next) {
                this->entry_pointer = this->entry_pointer->next;
            } else {
                do {
                    ++this->bucket_index;
                } while(this->bucket_index < this->table->bucket_count && this->table->buckets[this->bucket_index] == null);

                if(this->bucket_index < this->table->bucket_count) {
                    this->entry_pointer = this->table->buckets[this->bucket_index];
                } else {
                    this->entry_pointer = null;
                }
            }

            return *this;
        }

        Pair operator*() { return Pair(this->entry_pointer->hash, &this->entry_pointer->key, &this->entry_pointer->value); }
    };

    Allocator *allocator = Default_Allocator;
    Hash_Procedure hash;
    Comparison_Procedure compare;
    
    s64 count; // The total number of valid entries currently in the table.
    s64 bucket_count; // This internally gets rounded up to the next power of two, so that we can use the buket mask.
    s64 bucket_mask; // Masks all the lower bits to map a hash value into the bucket array.
    Entry **buckets; // For pointer stability, all entries must always be allocated. This is a bit sadge, but it has definitely bitten me in the arse before.

#if FOUNDATION_DEVELOPER
    Hash_Table_Stats stats;
#endif
    
    void create(s64 bucket_count, Hash_Procedure hash, Comparison_Procedure compare);
    void resize(s64 bucket_count);
    void destroy();

    void add(K const &k, V const &v);
    void remove(K const &k);
    V *push(K const &k);
    V *query(K const &k);

    Iterator begin();
    Iterator end();

    f64 fill_factor();

#if FOUNDATION_DEVELOPER
    f64 expected_number_of_collisions();
#endif
};

template<typename K, typename V>
struct Probed_Hash_Table {
    typedef u64(*Hash_Procedure)(K const &k);
    typedef b8(*Comparison_Procedure)(K const &lhs, K const &rhs);

    enum Entry_State {
        HASH_TABLE_ENTRY_Free    = 0,
        HASH_TABLE_ENTRY_Used    = 1,
        HASH_TABLE_ENTRY_Deleted = 2,
    };

    struct Entry {
        Entry_State state;
        u64 hash;
        K key;
        V value;
    };

    struct Pair {
        u64 hash;
        K *key;
        V *value;
        Pair(u64 hash, K *key, V *value) : hash(hash), key(key), value(value) {};
    };

    struct Iterator {
        Probed_Hash_Table<K, V> *table;
        s64 bucket_index;
        Entry *bucket_pointer;

        b8 operator==(Iterator const &it) { return this->bucket_pointer == it.bucket_pointer; }
        b8 operator!=(Iterator const &it) { return this->bucket_pointer != it.bucket_pointer; }
        Iterator &operator++() {
            do {
                ++this->bucket_index;
            } while(this->bucket_index < this->table->bucket_count && this->table->buckets[this->bucket_index].state != HASH_TABLE_ENTRY_Used);

            if(this->bucket_index < this->table->bucket_count) {
                this->bucket_pointer = &this->table->buckets[this->bucket_index];
            } else {
                this->bucket_pointer = null;
            }

            return *this;
        }

        Pair operator*() { return Pair(this->bucket_pointer->hash, &this->bucket_pointer->key, &this->bucket_pointer->value); }
    };
    
    Allocator *allocator = Default_Allocator;
    Hash_Procedure hash;
    Comparison_Procedure compare;

    s64 count; // The total number of valid entries currently in the table.
    s64 bucket_count; // This internally gets rounded up to the next power of two, so that we can use the bucket mask.
    s64 bucket_mask; // Masks all lower bits to map a hash value into the bucket array.
    Entry *buckets;

#if FOUNDATION_DEVELOPER
    Hash_Table_Stats stats;
#endif

    void create(s64 bucket_count, Hash_Procedure hash, Comparison_Procedure compare);
    void resize(s64 bucket_count);
    void destroy();

    void add(K const &k, V const &v);
    void remove(K const &k);
    V *push(K const &k);
    V *query(K const &k);

    Iterator begin();
    Iterator end();

    f64 fill_factor();
    
#if FOUNDATION_DEVELOPER
    f64 expected_number_of_collisions();
#endif
};

static inline u64 fnv1a_64(const void *data, u64 size);
static inline u64 murmur_64a(u64 key);

// Because C++ is a terrible language, we need to supply the template definitions in the header file for
// instantiation to work correctly... This feels horrible but still better than just inlining the code I guess.
#include "hash_table.inl"
