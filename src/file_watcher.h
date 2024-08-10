#pragma once

#include "foundation.h"
#include "memutils.h"
#include "string_type.h"

struct File_Watcher {
    struct Entry {
        string file_path;
        s64 last_modification_time; // Hardware time
        b8 file_exists;
    };
    
    Allocator *allocator = Default_Allocator;
    f32 check_interval_in_seconds = 0.5f;

    Resizable_Array<Entry> entries;    
    s64 last_check_time; // Hardware time

    void create();
    void destroy();
    
    void add_file_to_watch(string file_path);
    void remove_file_to_watch(string file_path);
    Resizable_Array<string> update(Allocator *result_allocator);
    
    // Sometimes user code knows about file changes and does not want notifications about these changes,
    // so this forces updates to the internal file times without notifications.
    void update_modification_times();
};