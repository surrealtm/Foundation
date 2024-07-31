#pragma once

#include "foundation.h"
#include "string_type.h"
#include "hash_table.h"
#include "error.h"

struct Package_Entry {
    s64 offset_in_bytes;
    s64 size_in_bytes;
};

struct Package {
#if IRONFORT_DEVELOPER
    b8 dealloacte_strings;
#endif

    Allocator *allocator;
    string file_data;
    Probed_Hash_Table<string, Package_Entry> table; // The key is the entry name ("file path"), the value is a substring view into the file_data.
    s64 header_size;
    s64 payload_size;
};

Error_Code create_package_from_file(Package *package, string file_path, Allocator *allocator);
void destroy_package(Package *package);
string query_package(Package *package, string name);

void create_empty_package(Package *package, s64 slots, Allocator *allocator);
void add_package_entry(Package *package, string name, string data);
void add_file_to_package(Package *package, string name, string file_path);
void write_package_to_file(Package *package, string file_path);
