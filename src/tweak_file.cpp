#include "tweak_file.h"
#include "fileio.h"
#include "os_specific.h"



/* ---------------------------------------------- Implementation ---------------------------------------------- */

#define report_tweak_error(file, line, format, ...) printf("[Error]: %.*s:%" PRId64 ": " format "\n", (u32) file->file_path.count, file->file_path.data, line, ##__VA_ARGS__)
#define report_tweak_error_without_line(file, format, ...) printf("[Error]: %.*s: " format "\n", (u32) file->file_path.count, file->file_path.data, ##__VA_ARGS__)
#define report_tweak_warning(file, line, format, ...) printf("[Warning]: %.*s:%" PRId64 ": " format "\n", (u32) file->file_path.count, file->file_path.data, line, ##__VA_ARGS__)
#define report_tweak_warning_without_line(file, format, ...) printf("[Warning]: %.*s: " format "\n", (u32) file->file_path.count, file->file_path.data, ##__VA_ARGS__)

static
Tweak_Service *find_tweak_service(Tweak_File *file, string name, s64 line) {
    for(s64 i = 0; i < file->services.count; ++i) {
        if(strings_equal(file->services[i].name, name)) return &file->services[i];
    }

    if(line != -1) report_tweak_warning(file, line, "The tweak service '%.*s' has not been registered.", (u32) name.count, name.data);
    return null;
}

static
Tweak_Variable *find_tweak_variable(Tweak_File *file, Tweak_Service *service, string name, s64 line) {
    for(s64 i = 0; i < service->variables.count; ++i) {
        if(strings_equal(service->variables[i].name, name)) return &service->variables[i];
    }

    if(line != -1) report_tweak_warning(file, line, "The tweak variable '%.*s' has not been registered in the service '%.*s'.", (u32) name.count, name.data, (u32) service->name.count, service->name.data);
    return null;
}

static
Tweak_Variable *register_tweak_variable(Tweak_File *file, string absolute_name, void *pointer, Tweak_Variable_Type type) {
    if(!absolute_name.count) return null;

    Tweak_Service *service;
    string variable_name;

    s64 last_slash = search_string_reverse(absolute_name, '/');
    if(absolute_name[0] == '/' && last_slash > 0) {
        string service_name = substring_view(absolute_name, 1, last_slash);
        service = find_tweak_service(file, service_name, -1);
        if(!service) service = register_tweak_service(file, service_name);

        variable_name = substring_view(absolute_name, last_slash + 1, absolute_name.count);
    } else {
        service = find_tweak_service(file, "."_s, -1);
        if(!service) service = register_tweak_service(file, "."_s);

        variable_name = absolute_name;
    }

    if(find_tweak_variable(file, service, variable_name, -1)) {
        report_tweak_error_without_line(file, "The tweak service '%.*s' already owns a variable called '%.*s'.", (u32) service->name.count, service->name.data, (u32) variable_name.count, variable_name.data);
        return null;
    }

    Tweak_Variable *variable = service->variables.push();
    variable->type    = type;
    variable->name    = copy_string(file->allocator, variable_name);
    variable->pointer = pointer;

    return variable;
}

static
void set_tweak_variable_from_string(Tweak_File *file, Tweak_Variable *variable, string _string, s64 line) {
    b8 success = true;

    switch(variable->type) {
    case TWEAK_VARIABLE_Void:
        break;
        
    case TWEAK_VARIABLE_U8:
        *(u8 *) variable->pointer = (u8) string_to_int(_string, &success);
        break;
    case TWEAK_VARIABLE_U16:
        *(u16 *) variable->pointer = (u16) string_to_int(_string, &success);
        break;
    case TWEAK_VARIABLE_U32:
        *(u32 *) variable->pointer = (u32) string_to_int(_string, &success);
        break;
    case TWEAK_VARIABLE_U64:
        *(u64 *) variable->pointer = (u64) string_to_int(_string, &success);
        break;
    case TWEAK_VARIABLE_S8:
        *(s8 *) variable->pointer = (s8) string_to_int(_string, &success);
        break;
    case TWEAK_VARIABLE_S16:
        *(s16 *) variable->pointer = (s16) string_to_int(_string, &success);
        break;
    case TWEAK_VARIABLE_S32:
        *(s32 *) variable->pointer = (s32) string_to_int(_string, &success);
        break;
    case TWEAK_VARIABLE_S64:
        *(s64 *) variable->pointer = (s64) string_to_int(_string, &success);
        break;
    case TWEAK_VARIABLE_F32:
        *(f32 *) variable->pointer = string_to_float(_string, &success);
        break;
    case TWEAK_VARIABLE_F64:
        *(f64 *) variable->pointer = string_to_double(_string, &success);
        break;
    case TWEAK_VARIABLE_String:
        if(_string.count >= 2 && _string[0] == '"' && _string[_string.count - 1] == '"') _string = substring_view(_string, 1, _string.count - 1);
        deallocate_string(file->allocator, (string *) variable->pointer);
        if(_string.count) *(string *) variable->pointer = copy_string(file->allocator, _string);
        break;
    }    
}

static
void print_tweak_variable_value(Ascii_Writer *writer, Tweak_Variable *variable) {
    switch(variable->type) {
    case TWEAK_VARIABLE_Void:
        break;

    case TWEAK_VARIABLE_U8:
        writer->write_u8(*(u8 *) variable->pointer);
        break;
    case TWEAK_VARIABLE_U16:
        writer->write_u16(*(u16 *) variable->pointer);
        break;
    case TWEAK_VARIABLE_U32:
        writer->write_u32(*(u32 *) variable->pointer);
        break;
    case TWEAK_VARIABLE_U64:
        writer->write_u64(*(u64 *) variable->pointer);
        break;
    case TWEAK_VARIABLE_S8:
        writer->write_s8(*(s8 *) variable->pointer);
        break;
    case TWEAK_VARIABLE_S16:
        writer->write_s16(*(s16 *) variable->pointer);
        break;
    case TWEAK_VARIABLE_S32:
        writer->write_s32(*(s32 *) variable->pointer);
        break;
    case TWEAK_VARIABLE_S64:
        writer->write_s64(*(s64 *) variable->pointer);
        break;
    case TWEAK_VARIABLE_F32:
        writer->write_f32(*(f32 *) variable->pointer);
        break;
    case TWEAK_VARIABLE_F64:
        writer->write_f64(*(f64 *) variable->pointer);
        break;
    case TWEAK_VARIABLE_String:
        writer->write_string(*(string *) variable->pointer);
        break;
    }
}

static
string read_tweak_line(string *file_data, s64 *line_number) {
    while(true) {
        if(!file_data->count) return ""_s;

        string line = read_next_line(file_data);
        ++(*line_number);

        s64 pound_index = search_string(line, '#');
        if(pound_index != -1) line = substring_view(line, 0, pound_index);

        line = trim_string(line);
        if(line.count) return line;
    }

    return ""_s;
}



/* ------------------------------------------------- User API ------------------------------------------------- */

void create_tweak_file(Tweak_File *file, Allocator *allocator, string file_path, Tweak_File_Reload_Callback reload_callback) {
    file->allocator              = allocator;
    file->file_path              = copy_string(file->allocator, file_path);
    file->last_hot_reload_check  = 0;
    file->last_modification_time = 0;
    file->reload_callback        = reload_callback;
    file->version                = 0;
    file->services.allocator     = file->allocator;
}

void destroy_tweak_file(Tweak_File *file) {
    for(Tweak_Service &service : file->services) {
        for(Tweak_Variable &variable : service.variables) {
            deallocate_string(file->allocator, &variable.name);        
        }
        
        deallocate_string(file->allocator, &service.name);
        service.variables.clear();
    }
    file->services.clear();
    
    deallocate_string(file->allocator, &file->file_path);
}

Tweak_Service *register_tweak_service(Tweak_File *file, string name) {
    Tweak_Service *service = find_tweak_service(file, name, -1);
    if(service) {
        report_tweak_warning_without_line(file, "The tweak file already owns a service called '%.*s'.", (u32) name.count, name.data);
        return service;
    }

    service = file->services.push();
    service->name = copy_string(file->allocator, name);
    service->variables.allocator = file->allocator;
    return service;
}

void register_tweak_variable(Tweak_File *file, string absolute_name, u8 *pointer) {
    register_tweak_variable(file, absolute_name, pointer, TWEAK_VARIABLE_U8);    
}


b8 read_tweak_file(Tweak_File *file) {
    //
    // Attempt to load the file from disk.
    //
    string file_data = os_read_file(Default_Allocator, file->file_path);
    if(!file_data.count) {
        report_tweak_error_without_line(file, "The file does not exist.");
        return false;
    }

    string original_file_data = file_data;
    defer { os_free_file_content(Default_Allocator, &original_file_data); };

    s64 line_number;

    //
    // Parse the version number.
    //
    string line = read_tweak_line(&file_data, &line_number);

    if(!line.count || line[0] != '[' || line[line.count - 1] != ']') {
        report_tweak_error(file, line_number, "The version number was expected in the first line.");
        return false;
    }

    b8 version_valid;
    string version_string = substring_view(line, 1, line.count - 1);
    file->version = (u32) string_to_int(version_string, &version_valid);

    if(!version_valid) {
        report_tweak_error(file, line_number, "The version number '%.*s' is invalid.", (u32) version_string.count, version_string.data);
        return false;
    }

    //
    // Parse the actual tweak services and variables.
    //

    Tweak_Service *current_service = null;
    b8 inside_global_service = true;
    b8 global_success = true;
    
    while(true) {
        line = read_tweak_line(&file_data, &line_number);
        if(!line.count) break;

        if(line.count >= 2 && line[0] == ':' && line[1] == '/') {
            //
            // Declaration of a new service
            //
            string service_name = substring_view(line, 2, line.count);
            current_service = find_tweak_service(file, service_name, line_number);
            inside_global_service = false;
        } else {
            //
            // Declaration of a variable
            //
            if(!current_service && inside_global_service) {
                current_service = find_tweak_service(file, "."_s, line_number);
                inside_global_service = false;
            }

            if(!current_service) {
                global_success = false;
                continue;
            }

            s64 space = search_string(line, ' ');
            if(space == -1) {
                report_tweak_error(file, line_number, "Expected a 'key value' set, seperated by a space, in this line.");
                global_success = false;
                continue;
            }

            // Make sure there is only one space character in the current line
            s64 reverse_space = search_string_reverse(line, ' ');
            if(reverse_space != space) {
                report_tweak_error(file, line_number, "Multiple values have been found in this line, expected only a single one.");
                global_success = false;
                continue;
            }

            string name = substring_view(line, 0, space);
            string value = substring_view(line, space + 1, line.count);

            Tweak_Variable *variable = find_tweak_variable(file, current_service, name, line_number);
            if(!variable) {
                global_success = false;
                continue;
            }

            set_tweak_variable_from_string(file, variable, value, line_number);
        }
    }

    //
    // Set up the data for hot reloading.
    //
    File_Information information = os_get_file_information(file->file_path);
    file->last_modification_time = information.last_modification_time;
    file->last_hot_reload_check  = os_get_hardware_time();

    return global_success;
}

void write_tweak_file(Tweak_File *file) {
    os_delete_file(file->file_path);

    Ascii_Writer writer;
    writer.create(file->file_path, 4096);

    writer.write_char('[');
    writer.write_u32(file->version);
    writer.write_char(']');
    writer.write_string(" # Version. Do not delete.\n\n"_s);

    for(Tweak_Service &service : file->services) {
        writer.write_string(":/"_s);
        writer.write_string(service.name);
        writer.write_string("\n"_s);

        for(Tweak_Variable &variable : service.variables) {
            writer.write_string(variable.name);
            writer.write_string(" "_s);
            print_tweak_variable_value(&writer, &variable);
            writer.write_string("\n"_s);
        }

        writer.write_string("\n"_s);
    }

    writer.flush();
    writer.destroy();
}

b8 maybe_reload_tweak_file(Tweak_File *file) {
    Hardware_Time now = os_get_hardware_time();
    b8 reloaded = false;

    if(os_convert_hardware_time(now - file->last_hot_reload_check, Seconds) > file->hot_reload_check_interval) {
        File_Information disk_info = os_get_file_information(file->file_path);
        if(disk_info.valid && disk_info.last_modification_time > file->last_modification_time) {
            reloaded = read_tweak_file(file);

            if(reloaded && file->reload_callback) file->reload_callback();
        } else {
            file->last_hot_reload_check = now;
        }
    }

    return reloaded;
}

b8 tweak_file_exists_on_disk(Tweak_File *file) {
    return os_file_exists(file->file_path);
}
