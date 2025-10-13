#include "file_watcher.h"
#include "os_specific.h"

void File_Watcher::create() {
    this->entries.allocator = this->allocator;
    this->last_check_time   = 0;
}

void File_Watcher::destroy() {
    for(Entry &all : this->entries) {
        deallocate_string(this->allocator, &all.file_path);
    }
    
    this->entries.clear();
}

void File_Watcher::add_file_to_watch(string file_path) {
    auto file_info = os_get_file_information(file_path);
    
    Entry *entry                  = this->entries.push();
    entry->file_path              = copy_string(this->allocator, file_path);
    entry->file_exists            = file_info.valid;
    entry->last_modification_time = file_info.last_modification_time;
}

void File_Watcher::remove_file_to_watch(string file_path) {
    b8 found_file = false;
    
    for(s64 i = 0; i < this->entries.count; ++i) {
        if(strings_equal(this->entries[i].file_path, file_path)) {
            deallocate_string(this->allocator, &this->entries[i].file_path);
            this->entries.remove(i);
            found_file = true;
            break;
        }
    }
    
    if(!found_file) {
        printf("Attempted to remove un-watched file '%.*s' from file watcher.\n", (u32) file_path.count, file_path.data);
    }
}

Resizable_Array<string> File_Watcher::update(Allocator *result_allocator) {
    Resizable_Array<string> result{};
    result.allocator = result_allocator;
    
    CPU_Time now = os_get_cpu_time();
    
    if(os_convert_cpu_time(now - this->last_check_time, Seconds) >= this->check_interval_in_seconds) {
        //
        // Update all file entries, and add them to the result if they have changed.
        //
        
        for(Entry &entry : this->entries) {
            auto file_info = os_get_file_information(entry.file_path);
            
            if(file_info.valid && file_info.last_modification_time > entry.last_modification_time) {
                entry.last_modification_time = file_info.last_modification_time;
                result.add(entry.file_path);
            }
            
            entry.file_exists = file_info.valid;
        }
        
        this->last_check_time = now;
    }
    
    return result;
}

void File_Watcher::update_modification_times() {
    for(Entry &entry : this->entries) {
        auto file_info = os_get_file_information(entry.file_path);
        
        if(file_info.valid) {
            entry.last_modification_time = file_info.last_modification_time;            
        }
        
        entry.file_exists = file_info.valid;
    }       
}