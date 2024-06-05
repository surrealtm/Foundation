#include "os_specific.h"
#include "memutils.h"

#include <linux/limits.h>
#include <sys/mman.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <sys/uio.h>
#include <unistd.h>
#include <errno.h>
#include <spawn.h>
#include <dlfcn.h>
#include <pwd.h>
#include <dirent.h>
#include <time.h>



/* ---------------------------------------------- Linux Helpers ---------------------------------------------- */

char *linux_maybe_expand_path_with_home_directory(Allocator *allocator, string file_path) {
    if(!file_path.count || file_path[0] != '~') return to_cstring(allocator, file_path);

    struct passwd *pw = getpwuid(getuid());
    
    assert(pw != NULL && pw->pw_dir != NULL);

    String_Builder builder;
    builder.create(allocator);
    builder.append_string(pw->pw_dir);
    builder.append_char('/');
    builder.append_string(substring_view(file_path, 1, file_path.count));
    builder.append_char('\0'); // We want this to be a cstring.
    return (char *) builder.finish().data;
}



/* --------------------------------------------------- Misc --------------------------------------------------- */

void os_debug_break() {
    __builtin_trap();
}

void os_terminate_process(u32 exit_code) {
    exit(exit_code);
}

void os_enable_high_resolution_clock() {
    // I don't think there is an equivalent procedure for this on linux as on windows.
}



/* ---------------------------------------------- Console Output ---------------------------------------------- */

b8 os_are_console_text_colors_supported() {
    return true;
}

void os_set_console_text_color(Console_Color_Code color) {
    // We use printf here to not mess with the internal buffer when using this procedure in between other
    // printf calls.
    switch(color) {
    case CONSOLE_COLOR_Dark_Red:
        printf("\x1B[31m");
        break;
    case CONSOLE_COLOR_Dark_Green:
        printf("\x1B[32m");
        break;
    case CONSOLE_COLOR_Dark_Blue:
        printf("\x1B[34m");
        break;
    case CONSOLE_COLOR_Red:
        printf("\x1B[91m");
        break;
    case CONSOLE_COLOR_Cyan:
        printf("\x1B[96m");
        break;
    case CONSOLE_COLOR_White:
        printf("\x1B[0m");
        break;
    default:
        printf("\x1B[0m");
        break;
    }
}

void os_write_to_console(const char *format, ...) {
    va_list args;
    va_start(args, format);
    vprintf(format, args);
    va_end(args);

	printf("\n");
}



/* ---------------------------------------------- Virtual Memory ---------------------------------------------- */

u64 os_get_page_size() {
    return getpagesize();
}

u64 os_get_committed_region_size(void *base) {
    // @Incomplete
    return 0;
}

void *os_reserve_memory(u64 reserved_size) {
    void *pointer = mmap(null, reserved_size, PROT_NONE, MAP_PRIVATE | MAP_ANONYMOUS, 0, 0);

    if(pointer == MAP_FAILED) {
        foundation_error("Failed to reserve %" PRIu64 " bytes of memory.", reserved_size);
    }

    return pointer;
}

void os_free_memory(void *base, u64 reserved_size) {
    assert(base != null);
    assert(reserved_size != null);
    
    munmap(base, reserved_size);
}

b8 os_commit_memory(void *base, u64 commit_size) {
    assert(base != null);
    assert(commit_size != null);

    int result = mprotect(base, commit_size, PROT_READ | PROT_WRITE);

    if(result != 0) {
        foundation_error("Failed to commit %" PRIu64 " bytes of memory.", commit_size);
    }

    memset(base, 0, commit_size); // Windows guarantees the committed pages to be zero-initialized, so we want to mimick that on linux. (This was ported from the compiler, so I'm assuming this is actually needed...)
    return result == 0;
}

void os_decommit_memory(void *base, u64 decommit_size) {
    assert(base != null);
    assert(decommit_size != null);

    int result = mprotect(base, decommit_size, PROT_NONE);

    if(result != 0) {
        foundation_error("Failed to decommit %" PRIu64 " bytes of memory.", decommit_size);
    }
}


u64 os_get_working_set_size() {
    // @Incomplete
    return 0;
}



/* ------------------------------------------------- File IO ------------------------------------------------- */

string os_read_file(Allocator *allocator, string file_path) {
    char *cstring = to_cstring(Default_Allocator, file_path);
    defer { free_cstring(Default_Allocator, cstring); };
    
    string result;

    FILE *file = fopen(cstring, "r");
    if(file) {
        fseek(file, 0, SEEK_END);
        long file_size = ftell(file);
        fseek(file, 0, SEEK_SET);

        result = allocate_string(allocator, file_size);
        fread(result.data, file_size, 1, file);
        fclose(file);
    } else {
        result = ""_s;
    }

    return result;
}

void os_free_file_content(Allocator *allocator, string *file_content) {
    deallocate_string(allocator, file_content);
}

b8 os_write_file(string file_path, string file_content, b8 append) {
    char *cstring = to_cstring(Default_Allocator, file_path);
    defer { free_cstring(Default_Allocator, cstring); };

    b8 success;
    
    FILE *file = fopen(cstring, append ? "a+" : "w+");
    if(file) {
        fwrite(file_content.data, file_content.count, 1, file);
        fflush(file);
        fclose(file);
        success = true;
    } else {
        success = false;
    }

    return success;
}

b8 os_create_directory(string file_path) {
    s64 parent_folder_end = os_search_path_for_directory_slash_reverse(file_path);
    if(parent_folder_end != -1) {
        string parent_folder = substring_view(file_path, 0, parent_folder_end);
        if(!os_create_directory(parent_folder)) return false;
    }
        
    char *cstring = to_cstring(Default_Allocator, file_path);
    b8 result = mkdir(cstring, 0700);
    free_cstring(Default_Allocator, cstring);

    return result;
}

b8 os_delete_file(string file_path) {
    char *cstring = to_cstring(Default_Allocator, file_path);
    b8 result = unlink(cstring) == 0;
    free_cstring(Default_Allocator, cstring);

    return result;
}

b8 os_file_exists(string file_path) {
    char *cstring = to_cstring(Default_Allocator, file_path);
    b8 result = access(cstring, F_OK) == 0;
    free_cstring(Default_Allocator, cstring);

    return result;
}

b8 os_directory_exists(string file_path) {
    struct stat sb;
    
    char *cstring = to_cstring(Default_Allocator, file_path);
    b8 result = stat(cstring, &sb) == 0 && S_ISDIR(sb.st_mode);
    free_cstring(Default_Allocator, cstring);
    
    return result;
}

File_Information os_get_file_information(string file_path) {
    struct stat sb;
    
    char *cstring = to_cstring(Default_Allocator, file_path);
    b8 result = stat(cstring, &sb) == 0 && !S_ISDIR(sb.st_mode);
    free_cstring(Default_Allocator, cstring);

    File_Information information;
    
    if(result) {
        information.file_size_in_bytes     = sb.st_size;
        information.creation_time          = sb.st_ctime;
        information.last_access_time       = sb.st_atime;
        information.last_modification_time = sb.st_mtime;
    } else {
        information = File_Information();
    }

    return information;
}



/* ------------------------------------------------ File Paths ------------------------------------------------ */

b8 os_looks_like_absolute_path(string file_path) {
    return file_path.count > 0 && file_path[0] == '/';
}

string os_convert_to_absolute_file_path(Allocator *allocator, string relative_path) {
    char *converted_relative_path = linux_maybe_expand_path_with_home_directory(Default_Allocator, relative_path);
    char absolute_path[PATH_MAX + 1];
    char *pointer = realpath(converted_relative_path, absolute_path);

    Default_Allocator->deallocate(converted_relative_path);
    
    if(!pointer) return ""_s;

    return copy_string(allocator, cstring_view(pointer));
}

s64 os_search_path_for_directory_slash_reverse(string file_path) {
    for(s64 i = file_path.count - 1; i >= 0; --i) {
        if(file_path.data[i] == '/') return i;
    }

    return -1;
}


void os_set_working_directory(string file_path) {
    char *cstring = to_cstring(Default_Allocator, file_path);
    chdir(cstring);
    free_cstring(Default_Allocator, cstring);
}

string os_get_working_directory() {
    char buffer[PATH_MAX + 1];
    char *pointer = getcwd(buffer, sizeof(buffer));
    return copy_string(Default_Allocator, cstring_view(pointer));
}

string os_get_executable_directory() {
    char path[PATH_MAX] = { 0 }; // readlink apparently doesn't null-terminate
    readlink("/proc/self/exe", path, ARRAY_COUNT(path));
    string path_view = cstring_view(path);
    s64 folder_length = os_search_path_for_directory_slash_reverse(path_view);
    if(folder_length == -1) folder_length = path_view.count;
    return make_string(Default_Allocator, (u8 *) path, folder_length);
}



static
void internal_get_files_in_folder(string file_path, Resizable_Array<string> *files, Files_In_Folder_Flags flags) {
    string concatenation = concatenate_strings(Default_Allocator, file_path, "/."_s);
    defer { deallocate_string(Default_Allocator, &concatenation); };

    char *cstring = to_cstring(Default_Allocator, concatenation);
    defer { free_cstring(Default_Allocator, cstring); };

    DIR *directory = opendir(cstring);
    if(directory) {
        while(struct dirent *entry = readdir(directory)) {
            string file_name_view = cstring_view(entry->d_name);

            if(entry->d_type == DT_DIR) {
                if(flags & FILES_IN_FOLDER_Files_And_Folders) {
                    if(flags & FILES_IN_FOLDER_Put_Original_Path_Into_Output_Paths) {
                        String_Builder builder;
                        builder.create(files->allocator);
                        builder.append_string(file_path);
                        builder.append_string("/");
                        builder.append_string(file_name_view);
                        files->add(builder.finish());
                    } else {
                        files->add(copy_string(files->allocator, file_name_view));
                    }
                }

                if(flags & FILES_IN_FOLDER_Recursive) {
                    String_Builder builder;
                    builder.create(Default_Allocator);
                    builder.append_string(file_path);
                    builder.append_string("/");
                    builder.append_string(file_name_view);
                    string folder_name = builder.finish();
                    internal_get_files_in_folder(folder_name, files, flags);
                    deallocate_string(Default_Allocator, &folder_name);
                }
            } else if(entry->d_type == DT_REG) {
                if(flags & FILES_IN_FOLDER_Put_Original_Path_Into_Output_Paths) {
                    String_Builder builder;
                    builder.create(files->allocator);
                    builder.append_string(file_path);
                    builder.append_string("/");
                    builder.append_string(file_name_view);
                    files->add(builder.finish());
                } else {
                    files->add(copy_string(files->allocator, file_name_view));
                }
            }
        }
        
        closedir(directory);
    }
}

Files_In_Folder os_get_files_in_folder(string file_path, Allocator *allocator, Files_In_Folder_Flags flags) {
    Resizable_Array<string> temp;
    temp.allocator = allocator;    
    internal_get_files_in_folder(file_path, &temp, flags);
    
    Files_In_Folder files;
    files.count = temp.count;
    files.file_paths = temp.data;
    return files;
}


void os_free_files_in_folder(Files_In_Folder *folder, Allocator *allocator) {
    allocator->deallocate(folder->file_paths);
    folder->file_paths = null;
    folder->count = 0;
}



                                           
/* -------------------------------------------------- Timing -------------------------------------------------- */

System_Time os_get_system_time() {
    time_t T = time(NULL);
    struct tm tm = *localtime(&T);

    System_Time system_time;
    system_time.year   = tm.tm_year + 1900;
    system_time.month  = tm.tm_mon + 1;
    system_time.day    = tm.tm_mday;
    system_time.hour   = tm.tm_hour;
    system_time.minute = tm.tm_min;
    system_time.second = tm.tm_sec;
    system_time.millisecond = 0;
    return system_time;
}

Hardware_Time os_get_hardware_time() {
    struct timespec time;
    clock_gettime(CLOCK_MONOTONIC, &time);
    return (Hardware_Time) (time.tv_sec * 1e9 + time.tv_nsec);
}

f64 os_convert_hardware_time(Hardware_Time time, Time_Unit unit) {
    f64 resolution_factor;

    switch(unit) {
    case Minutes:      resolution_factor = 60e9; break;
    case Seconds:      resolution_factor = 1e9; break;
    case Milliseconds: resolution_factor = 1e6; break;
    case Microseconds: resolution_factor = 1e3; break;
    case Nanoseconds:  resolution_factor = 1; break;
	default:           resolution_factor = 1; break;
    }

    return (f64) time / resolution_factor;
}

f64 os_convert_hardware_time(f64 time, Time_Unit unit) {
    f64 resolution_factor;

    switch(unit) {
    case Minutes:      resolution_factor = 60e9; break;
    case Seconds:      resolution_factor = 1e9; break;
    case Milliseconds: resolution_factor = 1e6; break;
    case Microseconds: resolution_factor = 1e3; break;
    case Nanoseconds:  resolution_factor = 1; break;
	default:           resolution_factor = 1; break;
    }

    return time / resolution_factor;    
}

void os_sleep(f64 seconds) {
    usleep((useconds_t) (seconds * 1000000.));
}

u64 os_get_cpu_cycle() {
    return __rdtsc();
}



/* ----------------------------------------------- Stack Trace ----------------------------------------------- */

Stack_Trace os_get_stack_trace() {
#if FOUNDATION_DEVELOPER
    // @Incomplete: https://man7.org/linux/man-pages/man3/backtrace.3.html
#else
    return Stack_Trace();
#endif
}

void os_free_stack_trace(Stack_Trace *trace) {
    for(s64 i = 0; i < trace->frame_count; ++i) {
        free(trace->frames[i].name);
        free(trace->frames[i].file);
    }

    free(trace->frames);
    trace->frames = null;
    trace->frame_count = 0;
}



/* --------------------------------------------- Bit Manipulation --------------------------------------------- */

u64 os_highest_bit_set(u64 value) {
    return __builtin_clzll(value);
}

u64 os_lowest_bit_set(u64 value) {
    return __builtin_ctzll(value);
}
