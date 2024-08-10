#pragma once

#include "foundation.h"
#include "hash_table.h"
#include "memutils.h"
#include "package.h"
#include "file_watcher.h"

#define INITIAL_CATALOG_SIZE 128

template<typename Asset>
struct Catalog {
    typedef Error_Code(*Create_Procedure)(Asset *asset, string file_content);
    typedef Error_Code(*Reload_Procedure)(Asset *asset, string file_content);
    typedef void(*Destroy_Procedure)(Asset *asset);

    struct Handle {
        Asset asset;
        string name;

#if FOUNDATION_DEVELOPER
        string file_path_on_disk;
#endif

        u16 references;
        b8 valid;
    };

    Allocator *allocator = Default_Allocator;
    Package *package = null;

    // The supplied name in the query() procedure get wrapped around by the directory and file extension strings
    // to form a (relative) file path, which is then either passed to the package or to the file system to
    // retrieve the actual file content.
    string directory;
    string file_extension;

    // For a nice API we need stable pointers to the underlying Asset, as well as fast indirections to get the
    // handle from both the name and the asset pointer.
    Linked_List<Handle> handles;
    Probed_Hash_Table<string,  Handle *> name_table;
    Probed_Hash_Table<Asset *, Handle *> pointer_table;

#if FOUNDATION_DEVELOPER
    File_Watcher file_watcher;
#endif

    // The actual asset managing code.
    Create_Procedure create_proc;
    Reload_Procedure reload_proc;
    Destroy_Procedure destroy_proc;
    
    void create_from_package(Package *package, string directory, string file_extension, Create_Procedure create, Reload_Procedure reload, Destroy_Procedure destroy);
    void create_from_file_system(string directory, string file_extension, Create_Procedure create, Reload_Procedure reload, Destroy_Procedure destroy);
    void destroy();
    
#if FOUNDATION_DEVELOPER
    void check_for_reloads();
#endif

    Asset *query(string name);
    void release(Asset *asset);

    string get_file_path(string name);
    string get_file_content(string name);
    void release_file_content(string *file_content);
};


// Because C++ is a terrible language, we need to supply the template definitions in the header file for
// instantiation to work correctly... This feels horrible but still better than just inlining the code I guess.
#include "catalog.inl"
