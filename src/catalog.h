#pragma once

#include "foundation.h"
#include "hash_table.h"
#include "memutils.h"
#include "package.h"
#include "file_watcher.h"

#define INITIAL_CATALOG_SIZE 128

#ifndef CATALOG_LOG_INFO
# define CATALOG_LOG_INFO(message, ...) printf(message "\n", ##__VA_ARGS__)
#endif

#ifndef CATALOG_LOG_ERROR
# define CATALOG_LOG_ERROR(message, ...) printf(message "\n", ##__VA_ARGS__)
#endif

template<typename Asset, typename Asset_Parameters = u8> // Asset_Parameters cannot be void (or we'll get compilation errors, so u8 kind of acts like there aren't any parameters...)
struct Catalog {
    struct Handle {
        Asset asset;
        string name;

#if FOUNDATION_DEVELOPER
        // For hot-loading
        Asset_Parameters parameters;
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

    virtual string make_complete_name(string name, Asset_Parameters) { return name; }
    virtual Error_Code create_proc(Asset *asset, string file_content, Asset_Parameters) = 0;
    virtual Error_Code reload_proc(Asset *asset, string file_content, Asset_Parameters) = 0;
    virtual void destroy_proc(Asset *asset) = 0;

    void create_from_package(Package *package, string directory, string file_extension);
    void create_from_file_system(string directory, string file_extension);
    void destroy();
    
#if FOUNDATION_DEVELOPER
    void check_for_reloads();
#endif

    Asset *internal_query(string name, Asset_Parameters parameters = {});
    Asset *query(string name, Asset_Parameters parameters) { return this->internal_query(name, parameters); }
    void release(Asset *asset);

    string get_file_path(string name);
    string get_file_content(string name);
    void release_file_content(string *file_content);
};

// Because C++ is a terrible language, we need to supply the template definitions in the header file for
// instantiation to work correctly... This feels horrible but still better than just inlining the code I guess.
#include "catalog.inl"
