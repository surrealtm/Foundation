#pragma once

#include "foundation.h"
#include "strings.h"

struct Allocator;


/* --------------------------------------------------- Misc --------------------------------------------------- */

void os_debug_break();



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
b8 os_commit_memory(void *base, u64 commit_size);
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



/* ------------------------------------------------ File Paths ------------------------------------------------ */

b8 os_looks_like_absolute_file_path(string file_path);
string os_convert_to_absolute_file_path(Allocator *allocator, string file_path);
s64 os_search_path_for_directory_slash_reverse(string file_path);

void os_set_working_directory(string file_path);
string os_get_working_directory();
string os_get_executable_directory();



/* -------------------------------------------------- Timing -------------------------------------------------- */

Hardware_Time os_get_hardware_time();
f64 os_convert_hardware_time(Hardware_Time input, Time_Unit unit);
f64 os_convert_hardware_time(f64 input, Time_Unit unit);
void os_sleep(f64 seconds);

u64 os_get_cpu_cycle();



/* ---------------------------------------------- Stack Walking ---------------------------------------------- */

struct Stack_Trace {
    struct Stack_Frame {
        char *name;
        char *file;
        s64 line;
    };

    Stack_Frame *frames;
    s64 frame_count;
};

Stack_Trace os_get_stack_trace();
void os_free_stack_trace(Stack_Trace *trace);



/* --------------------------------------------- Bit Manipulation --------------------------------------------- */

u64 os_highest_bit_set(u64 value);
u64 os_lowest_bit_set(u64 value);
