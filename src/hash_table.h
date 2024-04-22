#pragma once

#include "foundation.h"

// Apparently this needs to be in the global scope so that we can return it from functions?
// Or maybe the syntax is just shit idk.
template<typename K, typename V>
struct Hash_Table_Entry {
    Hash_Table_Entry *next;
    K key;
    V value;
};

template<typename K, typename V>
struct Hash_Table {
    typedef u64(*Hash_Table_Hash_Procedure)(K const &k);
    typedef b8(*Hash_Table_Comparison_Procedure)(K const &lhs, K const &rhs);
    
    s64 count;
    s64 bucket_count;
    b8 *bucket_occupied; // We don't explicitely allocate the first entry of each bucket (meaning: The bucket array contains the actual entries, not pointers to the first entries). To know whether an entry actually "exists", we require a boolean value. The next entries in each bucket are allocated, therefore implicitely indicating whether they exist or not.
    Hash_Table_Entry<K, V> *buckets;
    Hash_Table_Hash_Procedure hash;
    Hash_Table_Comparison_Procedure compare;

    Allocator *allocator = Default_Allocator;

    void create(s64 bucket_count, Hash_Table_Hash_Procedure hash, Hash_Table_Comparison_Procedure compare);
    void destroy();

    void add(K const &k, V const &v);
    void remove(K const &k);
    V *query(K const &k);

    Hash_Table_Entry<K, V> *find_entry(K const &k, u64 bucket_index);
};


// Because C++ is a terrible language, we need to supply the template definitions in the header file for
// instantiation to work correctly... This feels horrible but still better than just inlining the code I guess.
#include "hash_table.inl"
