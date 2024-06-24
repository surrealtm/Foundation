#pragma once

#include "foundation.h"
#include "memutils.h" // For Default_Allocator

//
// The so called Data Array is the data structure used in this engine to store entities of levels.
// The three main goals of this data format are:
//   1. Continuity in memory: We want to be able to quickly iterate over all entities (e.g. to render them, to
//      update them...), so all entities should be stored in a single chunk in memory without gaps.
//   2. Fast random access: We want to be able to quickly query specific entities based on some indexing type
//      (in this case an integer ID), in case some entity requires information about another to update correctly.
//   3. Support multiple "users": We want multiple developers to work on a singular level simultaneously. The
//      only critical thing here is that IDs must be unique so that when two editors create a new entity, these
//      entities don't have the same ID. All other operations (changing an entity, removing an entity) are all
//      handled well due to Subverions text merging behaviour.
//      To ensure unique IDs, each developer has a custom ID assigned to them and stored in the local tweak file.
//      That way, an entity gets the creating developer "attached" to it, so that two different developers can
//      never generate the same ID, therefore two developers can gracefully work on the same level.
//
// All of these goals are achieved by this data array. A short description to how it works:
//     The actual entities live in the "data"-blob. This is held continuous by moving entities around in memory
// when other entities get deleted. This allows array-like iteration over all entities.
//     The data structure then adds one layer of indirection above this data blob with the integer IDs. These
// ids are stable (meaning they never change over the lifetime of an entity) and should therefore be used to
// handle entity relations (since the raw pointers are unstable). This indirection is done by a simple array
// of mapping an ID (which is an index into the indirection array) to a data index (which is, well, an index
// into the data blob). This mapping will be adapted if the underlying entity moves around in the data array.
//     When an entity gets removed, we remember the now-free id (index into the indirection table) in a free-list,
// so that we can re-use that ID for the next created entity. This keeps the indirection table as tight as
// possible, but we cannot (and really need not) guarantee that it is gap-less.
//     To support different users, each user gets its own indirection array (to keep it as simple as possible).
// This means that we first extract the user out of a passed ID, then look at the index stored in the indirection
// table for that user, and then we find our raw data in the blob. The free-list also needs to be stored per-user,
// since it is linked to the indirection table.
//

typedef u32 Data_Array_Id;

#define INVALID_DATA_ARRAY_ID ((u32) -1)
#define INVALID_DATA_ARRAY_INDEX ((s64) -1)

template<typename T>
struct Data_Array {
    Allocator *allocator = Default_Allocator;

    s64 capacity; // Max capacity of items in this data array.
    s64 count; // Current number of valid items in this data array. They are stored at data[0] - data[count - 1]

    s64 *indirection; // Maps an id into the 'data' array.
    Data_Array_Id *id; // Stores the id of the data element at this index.
    T *data; // The actual continuous storage.

    Data_Array_Id *free_list; // A dynamic array of ids that are currently not used (and smaller than the highest id in use).
    s64 free_list_count;
    s64 free_list_capacity;
    
    void create(Allocator *allocator, s64 capacity);
    void destroy();

    Data_Array_Id push();
    T *push_with_id(Data_Array_Id id);

    void remove_by_id(Data_Array_Id id);
    void remove_by_index(s64 index);

    T *index(s64 index);
    T *query(Data_Array_Id id);

    b8 id_is_valid(Data_Array_Id id);

    void clear_free_list();
    void push_free_list(Data_Array_Id id);
    Data_Array_Id pop_free_list(s64 index = 0);
    void rebuild_free_list();
    void remove_from_free_list_if_exists(Data_Array_Id id);
};

// Because C++ is a terrible language, we need to supply the template definitions in the header file for
// instantiation to work correctly... This feels horrible but still better than just inlining the code I guess.
#include "data_array.inl"
