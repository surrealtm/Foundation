#pragma once

#include "foundation.h"
#include "string_type.h"

struct Allocator;


/* --------------------------------------------------- Misc --------------------------------------------------- */

#if FOUNDATION_WIN32
const char *win32_hresult_to_string(s64 hresult);
char *win32_last_error_to_string();
void win32_free_last_error_string(char *string);
#endif

void os_message_box(string message);
void os_debug_break();
void os_terminate_process(u32 exit_code);
void os_enable_high_resolution_clock();
void os_get_desktop_dpi(s32 *x, s32 *y);
b8 os_load_and_run_dynamic_library(string file_path, string procedure, void *argument);
b8 os_can_access_pointer(void *pointer);
s32 os_get_number_of_hardware_threads();
string os_get_user_name();



/* ---------------------------------------------- Console Output ---------------------------------------------- */

enum Console_Color_Code {
    CONSOLE_COLOR_Dark_Red,
    CONSOLE_COLOR_Dark_Green,
    CONSOLE_COLOR_Dark_Blue,
    CONSOLE_COLOR_Red,
    CONSOLE_COLOR_Cyan,
    CONSOLE_COLOR_White,
    CONSOLE_COLOR_Default,
};

b8 os_are_console_text_colors_supported();
void os_set_console_text_color(Console_Color_Code color);
void os_write_to_console(const char *format, ...);



/* ---------------------------------------------- Virtual Memory ---------------------------------------------- */

u64 os_get_page_size();
u64 os_get_committed_region_size(void *base);
void *os_reserve_memory(u64 reserved_size);
void os_free_memory(void *base, u64 reserved_size);
b8 os_commit_memory(void *base, u64 commit_size, b8 executable);
void os_decommit_memory(void *base, u64 decommit_size);

u64 os_get_working_set_size();



/* ------------------------------------------------- File IO ------------------------------------------------- */

string os_read_file(Allocator *allocator, string file_path);
void os_free_file_content(Allocator *allocator, string *file_content);
b8 os_write_file(string file_path, string file_content, b8 append);
b8 os_create_directory(string file_path);
b8 os_delete_file(string file_path);
b8 os_delete_directory(string file_path);
b8 os_file_exists(string file_path);
b8 os_directory_exists(string file_path);

struct File_Information {
    b8 valid;
    s64 file_size_in_bytes;
    s64 creation_time;
    s64 last_access_time;
    s64 last_modification_time;
};

File_Information os_get_file_information(string file_path);



/* ------------------------------------------------ File Paths ------------------------------------------------ */

b8 os_looks_like_absolute_file_path(string file_path);
string os_simplify_file_path(Allocator *allocator, string file_path); // Try to resolve '/../' parts of a file path, unify file path delimiters.
string os_convert_to_absolute_file_path(Allocator *allocator, string file_path); // Convert a relative file path to an absolute one.
s64 os_search_path_for_directory_slash_reverse(string file_path);

void os_set_working_directory(string file_path);
string os_get_working_directory();
string os_get_executable_directory();

enum Files_In_Folder_Flags {
    FILES_IN_FOLDER_Non_Recursive     = 0x1,
    FILES_IN_FOLDER_Recursive         = 0x2,
    FILES_IN_FOLDER_Just_Files        = 0x4,
    FILES_IN_FOLDER_Files_And_Folders = 0x8,
    FILES_IN_FOLDER_Put_Original_Path_Into_Output_Paths = 0x10, // Include the original path in the output to get the sort of 'absolute' path of the file.

    FILES_IN_FOLDER_Default           = FILES_IN_FOLDER_Recursive | FILES_IN_FOLDER_Files_And_Folders | FILES_IN_FOLDER_Put_Original_Path_Into_Output_Paths, // Non Recursive Files and Folders, including the original path
};

BITWISE(Files_In_Folder_Flags);

struct Files_In_Folder {
    string *file_paths;
    s64 count;
};

Files_In_Folder os_get_files_in_folder(string file_path, Allocator *allocator, Files_In_Folder_Flags flags = FILES_IN_FOLDER_Default);
void os_free_files_in_folder(Files_In_Folder *files, Allocator *allocator);



/* -------------------------------------------------- Timing -------------------------------------------------- */

struct System_Time {
    u16 millisecond;
    u16 second;
    u16 minute;
    u16 hour;
    u16 day;
    u16 month;
    u16 year;
};

System_Time os_get_system_time();
Hardware_Time os_get_hardware_time();
f64 os_convert_hardware_time(Hardware_Time input, Time_Unit unit);
f64 os_convert_hardware_time(f64 input, Time_Unit unit);
void os_sleep(f64 seconds);

u64 os_get_cpu_cycle();



/* ----------------------------------------------- System Calls ----------------------------------------------- */

s32 os_system_call(const char *executable, const char *arguments[], s64 argument_count, bool *found_application);
s32 os_system_call_wide_string(const wchar_t *command_line, bool *found_application); // Only supported on windows...



/* ---------------------------------------------- Stack Walking ---------------------------------------------- */

struct Stack_Frame {
    string source_file;
    s64 source_line;
    string description;
};

struct Stack_Trace {
    Stack_Frame *frames;
    s64 frame_count;
};

Stack_Trace os_get_stack_trace(Allocator *allocator, s64 skip);
void os_free_stack_trace(Allocator *allocator, Stack_Trace *trace);



/* --------------------------------------------- Bit Manipulation --------------------------------------------- */

u64 os_highest_bit_set(u64 value);
u64 os_lowest_bit_set(u64 value);
u64 os_count_leading_zeros(u64 value);
u64 os_count_trailing_zeros(u64 value);
u64 os_count_bits_set(u64 value);
b8 os_value_fits_in_bits(u64 value, u64 available_bits, b8 sign);
b8 os_value_is_power_of_two(u64 value);
u64 os_next_power_of_two(u64 value);
