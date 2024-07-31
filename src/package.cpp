#include "package.h"
#include "os_specific.h"
#include "fileio.h"



/* --------------------------------------------- Package Loading --------------------------------------------- */

Error_Code create_package_from_file(Package *package, string file_path, Allocator *allocator) {
#if IRONFORT_DEVELOPER
    package->dealloacte_strings = false;
#endif

    package->allocator = allocator;
    package->file_data = os_read_file(allocator, file_path);
    if(!package->file_data.count) {
        return ERROR_File_Not_Found;
    }

    Binary_Parser parser;
    parser.create_from_string(package->file_data);

    s64 entry_count = parser.read_s64();

    package->table.create(entry_count, string_hash, strings_equal);

    for(u32 i = 0; i < entry_count; ++i) {
        string name = parser.read_string();
        Package_Entry entry;
        entry.offset_in_bytes = parser.read_s64();
        entry.size_in_bytes   = parser.read_s64();
        package->table.add(name, entry);
    }

    package->header_size = parser.position;
    package->payload_size = parser.data.count - parser.position;

    return Success;
}

void destroy_package(Package *package) {
#if IRONFORT_DEVELOPER
    if(package->dealloacte_strings) {
        for(const auto &all : package->table) {
            deallocate_string(package->allocator, all.key);
        }
    }
#endif

    deallocate_string(package->allocator, &package->file_data);
    package->table.destroy();
}

string query_package(Package *package, string name) {
    Package_Entry *ptr = package->table.query(name);
    assert(ptr, "The package does not contain an entry called '%.*s'.", (u32) name.count, name.data);
    return string_view(package->file_data.data + package->header_size + ptr->offset_in_bytes, ptr->size_in_bytes);
}



/* --------------------------------------------- Package Creation --------------------------------------------- */

#if IRONFORT_DEVELOPER
void create_empty_package(Package *package, s64 slots, Allocator *allocator) {
    package->dealloacte_strings = true;
    package->allocator = allocator;
    package->file_data = ""_s;
    package->table.create(slots, string_hash, strings_equal);
    package->header_size = 0;
    package->payload_size = 0;
}

void add_package_entry(Package *package, string name, string data) {
    Package_Entry entry;
    entry.offset_in_bytes = package->payload_size;
    entry.size_in_bytes = data.count;

    package->payload_size += entry.size_in_bytes;

    if(package->file_data.count) {
        string new_file_data = concatenate_strings(package->allocator, package->file_data, data);
        deallocate_string(package->allocator, &package->file_data);
        package->file_data = new_file_data;
    } else {
        package->file_data = copy_string(package->allocator, data);
    }

    package->table.add(copy_string(package->allocator, name), entry);
}

void add_file_to_package(Package *package, string name, string file_path) {
    string file_content = os_read_file(&temp, file_path);
    if(!file_content.count) {
        log(LOG_ERROR, "Failed to add file '%.*s' from disk to package: The file does not exist.", (u32) file_path.count, file_path.data);
        return;
    }

    add_package_entry(package, name, file_content);
}

void write_package_to_file(Package *package, string file_path) {
    Binary_Writer writer;
    writer.create(file_path, 65535);
    writer.write_s64(package->table.count);

    for(const auto &all : package->table) {
        writer.write_string(*all.key);
        writer.write_s64(all.value->offset_in_bytes);
        writer.write_s64(all.value->size_in_bytes);
    }

    writer.write(package->file_data.data, package->file_data.count);

    writer.flush();
    writer.destroy();
}
#endif
