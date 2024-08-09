#include "os_specific.h"

template<typename Asset>
void Catalog<Asset>::create_from_package(Package *package, string directory, string file_extension, Create_Procedure create, Reload_Procedure reload, Destroy_Procedure destroy) {
    this->package        = package;
    this->directory      = copy_string(this->allocator, directory);
    this->file_extension = copy_string(this->allocator, file_extension);
    this->create_proc    = create;
    this->reload_proc    = reload;
    this->destroy_proc   = destroy;
    this->name_table.create(INITIAL_CATALOG_SIZE, string_hash, strings_equal);
    this->pointer_table.create(INITIAL_CATALOG_SIZE, [](Asset * const &key) -> u64 { return murmur_64a((u64) key); }, [](Asset * const &lhs, Asset * const &rhs) -> b8 { return lhs == rhs; });
}

template<typename Asset>
void Catalog<Asset>::create_from_file_system(string directory, string file_extension, Create_Procedure create, Reload_Procedure reload, Destroy_Procedure destroy) {    
    this->package        = null;
    this->directory      = copy_string(this->allocator, directory);
    this->file_extension = copy_string(this->allocator, file_extension);
    this->create_proc    = create;
    this->reload_proc    = reload;
    this->destroy_proc   = destroy;
    this->name_table.create(INITIAL_CATALOG_SIZE, string_hash, strings_equal);
    this->pointer_table.create(INITIAL_CATALOG_SIZE, [](Asset * const &key) -> u64 { return murmur_64a((u64) key); }, [](Asset * const &lhs, Asset * const &rhs) -> b8 { return lhs == rhs; });
}

template<typename Asset>
void Catalog<Asset>::destroy() {
    for(Handle &all : this->handles) {
        this->destroy_proc(&all.asset);
        deallocate_string(this->allocator, &all.name);
    }

    this->name_table.destroy();
    this->pointer_table.destroy();
    this->package = null;
    deallocate_string(this->allocator, &this->directory);
    deallocate_string(this->allocator, &this->file_extension);
}

template<typename Asset>
Asset *Catalog<Asset>::query(string name) {
    Handle **handle_ptr = this->name_table.query(name);
    if(handle_ptr != null) return &(*handle_ptr)->asset;

    Hardware_Time start_time = os_get_hardware_time();
    string file_content = this->get_file_content(name);    
    Handle *handle = this->handles.push();

    Error_Code error;
    if(file_content.count) {
        error = this->create_proc(&handle->asset, file_content);
    } else {
        error = ERROR_File_Not_Found;
    }

    handle->name       = copy_string(this->allocator, name);
    handle->references = 1;
    handle->valid      = error == Success;
    
    this->release_file_content(&file_content);
    
    this->name_table.add(handle->name, handle);
    this->pointer_table.add(&handle->asset, handle);

    Hardware_Time end_time = os_get_hardware_time();

    if(error == Success) {
        printf("Loaded asset '%.*s' (%.1fms).\n", (u32) name.count, name.data, os_convert_hardware_time(end_time - start_time, Milliseconds));
    } else {
        string es = error_string(error);
        printf("Failed to load asset '%.*s': %.*s.\n", (u32) name.count, name.data, (u32) es.count, es.data);
    }
    
    return &handle->asset;
}

template<typename Asset>
void Catalog<Asset>::release(Asset *asset) {
    if(!asset) return;

    Handle **handle_ptr = this->pointer_table.query(asset);
    if(handle_ptr == null) {
        printf("Attempted to release non-catalog asset.");
        return;
    }

    Handle *handle = *handle_ptr;
    --handle->references;

    if(handle->references == 0) {
        Hardware_Time start_time = os_get_hardware_time();
        
        string name = handle->name;
        this->destroy_proc(&handle->asset);
        this->name_table.remove(handle->name);
        this->pointer_table.remove(&handle->asset);
        this->handles.remove_value_pointer(handle);

        Hardware_Time end_time = os_get_hardware_time();
        
        printf("Unloaded asset '%.*s' (%.1fms).\n", (u32) name.count, name.data, os_convert_hardware_time(end_time - start_time, Milliseconds));
        deallocate_string(this->allocator, &name);
    }
}
    
template<typename Asset>
string Catalog<Asset>::get_file_content(string name) {
    s64 complete_path_length = this->directory.count + name.count + this->file_extension.count + 1;
    string complete_path;

    u8 path_buffer[256];
    if(complete_path_length <= 256) {
        complete_path = string_view(path_buffer, complete_path_length);
    } else {
        complete_path = allocate_string(Default_Allocator, complete_path_length);
    }

    memcpy(&complete_path.data[0], this->directory.data, this->directory.count);
    complete_path.data[this->directory.count] = '/';
    memcpy(&complete_path.data[this->directory.count + 1], name.data, name.count);
    memcpy(&complete_path.data[this->directory.count + 1 + name.count], this->file_extension.data, this->file_extension.count);

    printf("Complete path: %.*s\n", (u32) complete_path.count, complete_path.data);
    
    string file_content;
    if(this->package) {
        file_content = query_package(this->package, complete_path);
    } else {
        file_content = os_read_file(Default_Allocator, complete_path);
    }

    if(complete_path_length > 256) deallocate_string(Default_Allocator, &complete_path);
    
    return file_content;
}

template<typename Asset>
void Catalog<Asset>::release_file_content(string *file_content) {
    if(this->package) {
    } else {
        os_free_file_content(Default_Allocator, file_content);
    }
}
