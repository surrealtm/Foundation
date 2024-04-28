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
        K key;
        V value;
    };

    Allocator *allocator = Default_Allocator;
    Hash_Procedure hash;
    Comparison_Procedure compare;
    
    s64 count; // The total number of valid entries currently in the table.
    s64 bucket_count; // This internally gets rounded up to the next power of two, so that we can use the buket mask.
    s64 bucket_mask; // Masks all the lower bits to map a hash value into the bucket array.
    b8 *bucket_occupied; // We don't explicitely allocate the first entry of each bucket (meaning: The bucket array contains the actual entries, not pointers to the first entries). To know whether an entry actually "exists", we require a boolean value. The next entries in each bucket are allocated, therefore implicitely indicating whether they exist or not.
    Entry *buckets;

#if FOUNDATION_DEVELOPER
    Hash_Table_Stats stats;
#endif
    
    void create(s64 bucket_count, Hash_Procedure hash, Comparison_Procedure compare);
    void resize(s64 bucket_count);
    void destroy();

    void add(K const &k, V const &v);
    void remove(K const &k);
    V *query(K const &k);

    u64 find_bucket_index(K const &k);
    Entry *find_entry(K const &k, u64 bucket_index);

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
        K key;
        V value;
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
    V *query(K const &k);

#if FOUNDATION_DEVELOPER
    f64 expected_number_of_collisions();
#endif
};

u64 fnv1a_64(const void *data, u64 size);
u64 murmur_64a(u64 key);

// Because C++ is a terrible language, we need to supply the template definitions in the header file for
// instantiation to work correctly... This feels horrible but still better than just inlining the code I guess.
#include "hash_table.inl"
