#include "os_specific.h"
#include "memutils.h"

#include <stdarg.h>
#include <stdlib.h>
#include <linux/limits.h>
#include <sys/mman.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <sys/uio.h>
#include <sys/resource.h>
#include <unistd.h>
#include <errno.h>
#include <spawn.h>
#include <dlfcn.h>
#include <pwd.h>
#include <dirent.h>
#include <time.h>
#include <execinfo.h>
#include <X11/Xlib.h>
#include <X11/Xatom.h>
#include <X11/Xresource.h>


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

string linux_read_proc_file(const char *subfile, b8 process_specific) {
    //
    // Because linux is an absolute dumpster fire, proc files don't actually have a valid size (ftell will
    // always return 0), so it is actually expected to read from the file until we encounter EOF. No idea how
    // this is supposed to be good in any way...
    //
    string result;

    char file_path[256];

    if(process_specific) {
        pid_t pid = getpid();
        sprintf(file_path, "/proc/%d/%s", pid, subfile);
    } else {
        sprintf(file_path, "/proc/%s", subfile);
    }

    char internal_buffer[8129];
    
    FILE *file = fopen(file_path, "r");
    if(file) {       
        size_t length = fread(internal_buffer, 1, ARRAY_COUNT(internal_buffer), file);
        result = make_string(Default_Allocator, (u8 *) internal_buffer, length);
        fclose(file);
    } else {
        result = ""_s;
    }

    return result;
}

string linux_get_line(string *file_content) {
    s64 end = 0;

    while(end < file_content->count && file_content->data[end] != '\n') ++end;
    
    string result = substring_view(*file_content, 0, end);
    *file_content = substring_view(*file_content, end + 1, file_content->count);
    return result;
}

b8 linux_int_character(u8 c) {
    if(c >= '0' && c <= '9') return true;
    if(c >= 'a' && c <= 'f') return true;
    if(c >= 'A' && c <= 'F') return true;
    if(c == 'x') return true; // For hex prefix
    return false;
}

void linux_eat_character(string *line) {
    ++line->data;
    --line->count;
}

s64 linux_parse_int(string *line, b8 hex_as_default) {
    s64 end = 0;

    while(end < line->count && linux_int_character(line->data[end])) ++end;

    char internal_buffer[256];
    sprintf(internal_buffer, "%.*s", (u32) end, line->data);

    *line = substring_view(*line, end, line->count);
    
    return strtol(internal_buffer, 0, hex_as_default ? 16 : 0);
}



/* --------------------------------------------------- Misc --------------------------------------------------- */

void os_message_box(string message) {
    // Because Linux is a fucking shithole of an OS, there doesn't seem to be an easy way of creating a simple
    // message box like in windows. Instead, we'd need to construct and manually handle an entire fucking X11
    // window, which seems like wayyy too much effort...
}

void os_debug_break() {
    __builtin_trap();
}

void os_terminate_process(u32 exit_code) {
    exit(exit_code);
}

void os_enable_high_resolution_clock() {
    // I don't think there is an equivalent procedure for this on linux as on windows.
}

void os_get_desktop_dpi(s32 *x, s32 *y) {
    // Linux is such a shithole man.
    double dpi = 0.0;

    Display *display = XOpenDisplay(null);
    char *resourceString = XResourceManagerString(display);
    XrmInitialize();

    if(resourceString) {
        XrmDatabase db = XrmGetStringDatabase(resourceString);

        XrmValue value;
        char *type = NULL;
        if(XrmGetResource(db, "Xft.dpi", "String", &type, &value) == True) {
            if (value.addr) {
                dpi = atof(value.addr);
            }
        }
    }

    XCloseDisplay(display);
    
    *x = (s32) dpi;
    *y = (s32) dpi;
}

b8 os_load_and_run_dynamic_library(string file_path, string procedure, void *argument) { // @Incomplete
    return false;
}

b8 os_can_access_pointer(void *pointer) {
    char local_buffer[8];

    struct iovec local, remote;
    local.iov_base = &local_buffer;
    local.iov_len  = sizeof(local_buffer);

    remote.iov_base = pointer;
    remote.iov_len  = sizeof(local_buffer);

    size_t read = process_vm_readv(getpid(), &local, 1, &remote, 1, 0);   
    return read != -1;
}

s32 os_get_number_of_hardware_threads() {
    return sysconf(_SC_NPROCESSORS_ONLN);
}

string os_get_user_name() { // @Incomplete
    return ""_s;
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
    string file_content = linux_read_proc_file("maps", true);

    string original_file_content = file_content;
    defer { deallocate_string(Default_Allocator, &original_file_content); };

    u64 size = 0;
    
    string line;
    while((line = linux_get_line(&file_content)).count) {
        u64 low  = linux_parse_int(&line, true);
        linux_eat_character(&line);
        u64 high = linux_parse_int(&line, true);
        
        if((s64) base == low) {
            size = high - low;
            break;
        }
    }
    
    return size;
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

b8 os_commit_memory(void *base, u64 commit_size, b8 executable) {
    assert(base != null);
    assert(commit_size != null);

    int result = mprotect(base, commit_size, executable ? (PROT_EXEC | PROT_READ | PROT_WRITE) : (PROT_READ | PROT_WRITE));

    if(result != 0) {
        foundation_error("Failed to commit %" PRIu64 " bytes of memory.", commit_size);
    }

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
    //
    // Linux doesn't actually calculate the "working-set-size", but instead the "resident-set-size". The latter
    // one is the number of kilobytes of pages that actually reside in main memory, which seems close enough for
    // a rough performance overview...
    // Note: This only gives us the MAX Rss, not the current one...
    //
    struct rusage usage;
    getrusage(RUSAGE_SELF, &usage);
    return usage.ru_maxrss * ONE_KILOBYTE;
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
        information.valid                  = true;
        information.file_size_in_bytes     = sb.st_size;
        information.creation_time          = sb.st_ctime;
        information.last_access_time       = sb.st_atime;
        information.last_modification_time = sb.st_mtime;
    } else {
        information.valid = false;
    }

    return information;
}



/* ------------------------------------------------ File Paths ------------------------------------------------ */

b8 os_looks_like_absolute_file_path(string file_path) {
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



/* ----------------------------------------------- System Calls ----------------------------------------------- */

s32 os_system_call(const char *executable, const char *arguments[], s64 argument_count) {
    const char **complete_arguments = (const char **) Default_Allocator->allocate(sizeof(char *) * (argument_count + 1)); // @@Leak

    complete_arguments[0] = executable;
    for(s64 i = 0; i < argument_count; ++i) complete_arguments[i + 1] = arguments[i];
    complete_arguments[argument_count + 1] = 0;
    
    pid_t pid = 0;
    if(posix_spawnp(&pid, executable, NULL, NULL, (char * const *) complete_arguments, __environ) != 0) {
        return -1;
    }

    int exit_code;
    waitpid(pid, &exit_code, 0);

    return exit_code;
}



/* ----------------------------------------------- Stack Trace ----------------------------------------------- */

Stack_Trace os_get_stack_trace() {
#if FOUNDATION_DEVELOPER
    const s64 max_frames_to_capture = 256; // We don't know in advance how many frames there are going to be (and we don't want to iterate twice), so just preallocate a max number.


    void *return_addresses[max_frames_to_capture];
    int frame_count = backtrace(return_addresses, max_frames_to_capture);

    char **symbol_names = backtrace_symbols(return_addresses, frame_count);

    Stack_Trace trace;
    trace.frames = (Stack_Trace::Stack_Frame *) malloc(frame_count * sizeof(Stack_Trace::Stack_Frame));
    trace.frame_count = frame_count - 1; // The first frame of backtrace() is os_get_stack_trace, which we obviously don't want.

    s64 frame_index = 0;
    for(s64 i = 1; i < frame_count; ++i) { // The first frame of backtrace() is os_get_stack_trace, which we obviously don't want.
        // backtrace() only gives us one symbol name, so I guess that'll have to do here...
        trace.frames[frame_index].name = (char *) malloc(strlen(symbol_names[i]) + 1);
        strcpy(trace.frames[frame_index].name, symbol_names[i]);
        trace.frames[frame_index].file = null;
        ++frame_index;
    }

    return trace;
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
    return 63 - __builtin_clzll(value);
}

u64 os_lowest_bit_set(u64 value) {
    return __builtin_ctzll(value);
}

b8 os_value_fits_in_bits(u64 value, u64 available_bits, b8 sign) {
    if(value == 0 || available_bits == 64) return true;

    union {
        u64 _unsigned;
        s64 _signed;
    } _union;

    _union._unsigned = value;
    
    if(sign) {
        u64 highest = (1ULL << (available_bits - 1)) - 1;
        s64 lowest  = 1ULL << (available_bits - 1);
        return (_union._signed >= 0 && _union._unsigned <= highest) || (_union._signed < 0 && -_union._signed <= lowest);
    } else {
        // Also allow unsigned variables to store negative values if they are in the negative
        // signed range, for convenience in the compiler's type checker.
        u64 highest = (1ULL << available_bits) - 1;
        s64 lowest  = 1ULL << (available_bits - 1);
        return (_union._signed >= 0 && _union._unsigned <= highest) ||(_union._unsigned < 0 && -_union._signed <= lowest);
    }

}
