#pragma once

#include "foundation.h"
#include "string_type.h"
#include "memutils.h"

typedef void(*Tweak_File_Reload_Callback)();

enum Tweak_Variable_Type {
    TWEAK_VARIABLE_Void,
    TWEAK_VARIABLE_U8,    
    TWEAK_VARIABLE_U16,    
    TWEAK_VARIABLE_U32,    
    TWEAK_VARIABLE_U64,    
    TWEAK_VARIABLE_S8,    
    TWEAK_VARIABLE_S16,    
    TWEAK_VARIABLE_S32,    
    TWEAK_VARIABLE_S64,
    TWEAK_VARIABLE_F32,
    TWEAK_VARIABLE_F64,
    TWEAK_VARIABLE_String,
};

struct Tweak_Variable {
    Tweak_Variable_Type type;
    string name;
    void *pointer;    
};

struct Tweak_Service {
    string name;
    Resizable_Array<Tweak_Variable> variables;
};

struct Tweak_File {
    Allocator *allocator;
    string file_path;
    
    f32 hot_reload_check_interval = 0.5f; // In seconds
    s64 last_hot_reload_check; // Hardware time
    s64 last_modification_time; // Hardware time
    Tweak_File_Reload_Callback reload_callback;    
    
    u32 version;
    Resizable_Array<Tweak_Service> services;    
};

void create_tweak_file(Tweak_File *file, string file_path, Allocator *allocator, Tweak_File_Reload_Callback reload_callback = null);
void destroy_tweak_file(Tweak_File *file);
Tweak_Service *register_tweak_service(Tweak_File *file, string name);
void register_tweak_variable(Tweak_File *file, string absolute_name, u8 *pointer);

b8 read_tweak_file(Tweak_File *file);
void write_tweak_file(Tweak_File *file);
b8 maybe_reload_tweak_file(Tweak_File *file);
b8 tweak_file_exists_on_disk(Tweak_File *file);
void debug_print_tweak_file(Tweak_File *file);
