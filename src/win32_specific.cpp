#include "foundation.h"
#include "os_specific.h"
#include "memutils.h"

#define WIN32_MEAN_AND_LEAN
#include <wchar.h> // Apparently some fucking windows header requires this somethimes or something I don't even know I don't want to have to deal with this shit.
#include <Windows.h>
#include <psapi.h> // For getting memory usage
#include <dbghelp.h> // For stack walking



/* ---------------------------------------------- Win32 Helpers ---------------------------------------------- */

char *win32_last_error_to_string() {
	DWORD error = GetLastError();
	if(!error) return null;

	char *messageBuffer = null;

	size_t size = FormatMessageA(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS, 
		null, error, 0, (LPSTR) &messageBuffer, 0, null);

	return messageBuffer;
}

void win32_free_last_error_string(char *string) {
	if(string) LocalFree(string);
}

static
char *win32_copy_cstring(const char *cstring) {
    s64 length = strlen(cstring);
    char *ptr = (char *) malloc(length + 1);
    strcpy_s(ptr, length + 1, cstring);
    return ptr;
}



/* --------------------------------------------------- Misc --------------------------------------------------- */

void os_debug_break() {
    DebugBreak();
}

void enable_high_resolution_clock() {
    timeBeginPeriod(1);
}



/* ---------------------------------------------- Console Output ---------------------------------------------- */

b8 os_are_console_text_colors_supported() {
    b8 supported;

    u32 mode;
    if(GetConsoleMode(GetStdHandle(STD_OUTPUT_HANDLE), &mode)) {
        supported = (mode & ENABLE_VIRTUAL_TERMINAL_PROCESSING) && (mode & ENABLE_PROCESSED_OUTPUT);
    } else {
        supported = false;
    }

    return supported;
}

void os_set_console_text_color(Console_Color_Code color) {
    u32 buffer_length;
    char buffer[6];
    buffer[0] = 0x1b; // ESC
    buffer[1] = 0x5b; // [

    switch(color) {
    case CONSOLE_COLOR_Dark_Red:
        buffer[2] = '3';
        buffer[3] = '1';
        buffer_length = 4;
        break;
    case CONSOLE_COLOR_Dark_Green:
        buffer[2] = '3';
        buffer[3] = '2';
        buffer_length = 4;
        break;
    case CONSOLE_COLOR_Dark_Blue:
        buffer[2] = '3';
        buffer[3] = '4';
        buffer_length = 4;
        break;
    case CONSOLE_COLOR_Red:
        buffer[2] = '9';
        buffer[3] = '1';
        buffer_length = 4;
        break;
    case CONSOLE_COLOR_Cyan:
        buffer[2] = '9';
        buffer[3] = '6';
        buffer_length = 4;
        break;
    case CONSOLE_COLOR_White:
        buffer[2] = '9';
        buffer[3] = '7';
        buffer_length = 4;
        break;        
    case CONSOLE_COLOR_Default:
    default:
        buffer[2] = '0';
        buffer_length = 3;
        break;
    }

    buffer[buffer_length] = 'm';
    ++buffer_length;

    printf("%.*s", buffer_length, buffer); // Don't mess with the internal printf thingy...
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
	SYSTEM_INFO system_info;
	GetSystemInfo(&system_info);
	return system_info.dwPageSize;
}

u64 os_get_committed_region_size(void *base) {
	MEMORY_BASIC_INFORMATION information;
	SIZE_T result = VirtualQuery(base, &information, sizeof(information));
	if(!result) {
		char *error = win32_last_error_to_string();
		foundation_error("Failed to query committed region size: %s.", error);
		win32_free_last_error_string(error);
		return 0;
	}

	return information.State == MEM_COMMIT ? information.RegionSize : 0;
}

void *os_reserve_memory(u64 reserved_size) {
	assert(reserved_size != 0);

	void *base = VirtualAlloc(null, reserved_size, MEM_RESERVE, PAGE_NOACCESS);

	if(!base) {
		char *error = win32_last_error_to_string();
		foundation_error("Failed to reserve %" PRIu64 " bytes of memory: %s.", reserved_size, error);
		win32_free_last_error_string(error);
	}

	return base;
}

void os_free_memory(void *base, u64 reserved_size) {
	assert(base != null);
	assert(reserved_size != 0);

	b8 result = VirtualFree(base, 0, MEM_RELEASE);

	if(!result) {
		char *error = win32_last_error_to_string();
		foundation_error("Failed to free %" PRIu64 " bytes of memory: %s.", reserved_size, error);
		win32_free_last_error_string(error);
	}
}

b8 os_commit_memory(void *address, u64 commit_size) {
	assert(address != null);
	assert(commit_size != 0);

	void *result = VirtualAlloc(address, commit_size, MEM_COMMIT, PAGE_READWRITE);

	if(!result) {
		char *error = win32_last_error_to_string();
		foundation_error("Failed to commit %" PRIu64 " bytes of memory: %s.", commit_size, error);
		win32_free_last_error_string(error);
	}

	return result != null;
}

void os_decommit_memory(void *address, u64 decommit_size) {
	assert(address != null);
	assert(decommit_size != 0);

	b8 result = VirtualFree(address, decommit_size, MEM_DECOMMIT); // This should just decommit parts of the virtual address space, we'll free it later in os_free_memory.

	if(!result) {
		char *error = win32_last_error_to_string();
		foundation_error("Failed to decommit %" PRIu64 " bytes of memory: %s.", decommit_size, error);
		win32_free_last_error_string(error);
	}
}

u64 os_get_working_set_size() {
    PROCESS_MEMORY_COUNTERS_EX counters = { 0 };
    if(GetProcessMemoryInfo(GetCurrentProcess(), (PROCESS_MEMORY_COUNTERS *) &counters, sizeof(counters)))
        return counters.PrivateUsage;
    else
        return 0;
}



/* ------------------------------------------------- File IO ------------------------------------------------- */

string os_read_file(Allocator *allocator, string file_path) {
	string file_content = { 0 };
	char *cstring = to_cstring(Default_Allocator, file_path);

	HANDLE file_handle = CreateFileA(cstring, GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, null, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, null);
	if(file_handle != INVALID_HANDLE_VALUE) {
		file_content.count = GetFileSize(file_handle, null);
		file_content.data  = (u8 *) allocator->allocate(file_content.count);

		if(!ReadFile(file_handle, file_content.data, (u32) file_content.count, null, null)) {
			allocator->deallocate(file_content.data);
			file_content = { 0 };
		}
	}

	CloseHandle(file_handle);

	free_cstring(Default_Allocator, cstring);
	return file_content;
}

void os_free_file_content(Allocator *allocator, string *file_content) {
	allocator->deallocate(file_content->data);
	file_content->count = 0;
	file_content->data  = 0;
}

b8 os_write_file(string file_path, string file_content, b8 append) {
	b8 success = false;
	char *cstring = to_cstring(Default_Allocator, file_path);

	HANDLE file_handle = CreateFileA(cstring, GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, null, append ? OPEN_ALWAYS : CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, null);

	if(file_handle != INVALID_HANDLE_VALUE) {
		u32 file_offset = 0;
		if(append) file_offset = SetFilePointer(file_handle, 0, null, FILE_END);

		success = WriteFile(file_handle, file_content.data, (u32) file_content.count, null, null);
	}

	CloseHandle(file_handle);

	free_cstring(Default_Allocator, cstring);
	return success;
}

b8 os_create_directory(string file_path) {
    s64 parent_folder_end = os_search_path_for_directory_slash_reverse(file_path);
    if(parent_folder_end != -1) {
        string parent_folder = substring_view(file_path, 0, parent_folder_end);
        if(!os_create_directory(parent_folder)) return false;
    }
        
    char *path_cstring = to_cstring(Default_Allocator, file_path);
    b8 result = CreateDirectoryA(path_cstring, null);
    free_cstring(Default_Allocator, path_cstring);

    return result;
}

b8 os_delete_file(string file_path) {
	char *cstring = to_cstring(Default_Allocator, file_path);
	b8 success = DeleteFileA(cstring);
	free_cstring(Default_Allocator, cstring);
	return success;
}

b8 os_delete_directory(string file_path) {
	// The Win32 API also has a RemoveDirectoryA procedure, but that only works on empty folders, which we don't
    // require in this module procedure, so instead we use a PowerShell procedure... Sigh.
    // SHFileOperation requires the strings to be double-null-terminated for some fucking reason...

    char *cstring = (char *) Default_Allocator->allocate(file_path.count + 2);
    memcpy(cstring, file_path.data, file_path.count);
    cstring[file_path.count + 0] = 0;
    cstring[file_path.count + 1] = 0;
    
    SHFILEOPSTRUCTA file_operation;
    file_operation.hwnd   = null;
    file_operation.wFunc  = FO_DELETE;
    file_operation.pFrom  = cstring;
    file_operation.pTo    = null;
    file_operation.fFlags = FOF_NOCONFIRMATION | FOF_NOERRORUI | FOF_SILENT;

    // @Cleanup: This just randomly fails sometimes with the weirdest return values...
    // I am not sure what the fuck is happening here, so maybe we need to find something else man.
	//  ^ The above message was copied from the prometheus module, not sure...
	u32 result = SHFileOperationA(&file_operation);

	Default_Allocator->deallocate(cstring);

    return result == 0;
}

b8 os_file_exists(string file_path) {
	char *cstring = to_cstring(Default_Allocator, file_path);
	
	u32 attributes = GetFileAttributesA(cstring);
	b8 success = attributes != -1 && !(attributes & FILE_ATTRIBUTE_DIRECTORY);
	
	free_cstring(Default_Allocator, cstring);
	return success;
}

b8 os_directory_exists(string file_path) {
	char *cstring = to_cstring(Default_Allocator, file_path);
	
	u32 attributes = GetFileAttributesA(cstring);
	b8 success = attributes != -1 && attributes & FILE_ATTRIBUTE_DIRECTORY;
	
	free_cstring(Default_Allocator, cstring);
	return success;	
}



/* ------------------------------------------------ File Paths ------------------------------------------------ */

b8 os_looks_like_absolute_file_path(string file_path) {
	return file_path.count > 2 && file_path[1] == ':' && (file_path[2] == '/' || file_path[2] == '\\');
}

string os_convert_to_absolute_file_path(Allocator *allocator, string file_path) {
    char *cstring = to_cstring(Default_Allocator, file_path);
	u32 buffer_size = GetFullPathNameA(cstring, 0, null, null);
    string result = allocate_string(allocator, buffer_size);
    GetFullPathNameA(cstring, buffer_size, (LPSTR) result.data, null);
	free_cstring(Default_Allocator, cstring);
    return result;
}

s64 os_search_path_for_directory_slash_reverse(string file_path) {
    for(s64 i = file_path.count - 1; i >= 0; --i) {
        if(file_path.data[i] == '\\' || file_path.data[i] == '/') return i;
    }

    return -1;
}


void os_set_working_directory(string file_path) {
    char *cstring = to_cstring(Default_Allocator, file_path);
    SetCurrentDirectoryA(cstring);
    free_cstring(Default_Allocator, cstring);
}

string os_get_working_directory() {
    u8 path[MAX_PATH];
    auto path_length = GetCurrentDirectoryA(MAX_PATH, (LPSTR) path);
    return make_string(Default_Allocator, path, path_length);
}

string os_get_executable_directory() {
    u8 path[MAX_PATH];
    auto path_length  = GetModuleFileNameA(null, (LPSTR) path, MAX_PATH);
    string path_view  = string_view(path, path_length);
    s64 folder_length = os_search_path_for_directory_slash_reverse(path_view);
    if(folder_length == -1) folder_length = path_view.count;
    return make_string(Default_Allocator, path, folder_length);
}



/* -------------------------------------------------- Timing -------------------------------------------------- */

LARGE_INTEGER __win32_performance_frequency;
b8 __win32_performance_frequency_set = false;

Hardware_Time os_get_hardware_time() {
    LARGE_INTEGER counter;
    QueryPerformanceCounter(&counter);
    return counter.QuadPart;
}

f64 os_convert_hardware_time(Hardware_Time time, Time_Unit unit) {
    if(!__win32_performance_frequency_set) __win32_performance_frequency_set = QueryPerformanceFrequency(&__win32_performance_frequency);

    f64 resolution_factor;

    switch(unit) {
    case Minutes:      resolution_factor = 1.0 / 60.0; break;
    case Seconds:      resolution_factor = 1.0; break;
    case Milliseconds: resolution_factor = 1000.0; break;
    case Microseconds: resolution_factor = 1000000.0; break;
    case Nanoseconds:  resolution_factor = 1000000000.0; break;
	default:           resolution_factor = 1; break;
    }
    
    return (f64) time / (f64) (__win32_performance_frequency.QuadPart) * resolution_factor;
}

f64 os_convert_hardware_time(f64 time, Time_Unit unit) {
    if(!__win32_performance_frequency_set) __win32_performance_frequency_set = QueryPerformanceFrequency(&__win32_performance_frequency);

    f64 resolution_factor;

    switch(unit) {
    case Minutes:      resolution_factor = 1.0 / 60.0; break;
    case Seconds:      resolution_factor = 1.0; break;
    case Milliseconds: resolution_factor = 1000.0; break;
    case Microseconds: resolution_factor = 1000000.0; break;
    case Nanoseconds:  resolution_factor = 1000000000.0; break;
	default:           resolution_factor = 1; break;
    }
    
    return time / (f64) (__win32_performance_frequency.QuadPart) * resolution_factor;
}

void os_sleep(f64 seconds) {
    Sleep((DWORD) round(seconds * 1000));
}


u64 os_get_cpu_cycle() {
	return __rdtsc();
}



/* ----------------------------------------------- Stack Trace ----------------------------------------------- */

Stack_Trace os_get_stack_trace() {
    const s64 max_frames_to_capture = 256; // We don't know in advance how many frames there are going to be (and we don't want to iterate twice), so just preallocate a max number.

    //
    // We don't use allocators here to have as little overhead as possible, and
    // because we may actually have a stack overflow when calling this procedure
    // inside an allocator callback...
    //

    Stack_Trace trace;
    trace.frames = (Stack_Trace::Stack_Frame *) malloc(max_frames_to_capture * sizeof(Stack_Trace::Stack_Frame));
    trace.frame_count = 0;

    HANDLE process = GetCurrentProcess();
    HANDLE thread = GetCurrentThread();
    
    SymInitialize(process, null, true);

    CONTEXT context;
    RtlCaptureContext(&context);

    STACKFRAME64 stack_frame     = { 0 };
    stack_frame.AddrPC.Offset    = context.Rip;
    stack_frame.AddrPC.Mode      = AddrModeFlat;
    stack_frame.AddrStack.Offset = context.Rsp;
    stack_frame.AddrStack.Mode   = AddrModeFlat;
    stack_frame.AddrFrame.Offset = context.Rbp;
    stack_frame.AddrFrame.Mode   = AddrModeFlat;

    char symbol_buffer[sizeof(IMAGEHLP_SYMBOL64) + 256];
    IMAGEHLP_SYMBOL64 *symbol = (IMAGEHLP_SYMBOL64 *) symbol_buffer;
    symbol->SizeOfStruct      = sizeof(IMAGEHLP_SYMBOL64);
    symbol->MaxNameLength     = 255;

    IMAGEHLP_LINE64 line;
    b8 is_self_frame = true;
    
    while(trace.frame_count < max_frames_to_capture && StackWalk64(IMAGE_FILE_MACHINE_AMD64, process, thread, &stack_frame, &context, null, SymFunctionTableAccess64, SymGetModuleBase64, null)) {
        if(is_self_frame) {
            // We don't care about the stack frame for 'os_get_stack_trace' itself.
            is_self_frame = false;
            continue;
        }

        DWORD64 symbol_displacement;
        DWORD line_displacement;

        if(SymGetSymFromAddr64(process, stack_frame.AddrPC.Offset, &symbol_displacement, symbol) && SymGetLineFromAddr64(process, stack_frame.AddrPC.Offset, &line_displacement, &line)) {
            trace.frames[trace.frame_count].name = win32_copy_cstring(symbol->Name);
            trace.frames[trace.frame_count].file = win32_copy_cstring(line.FileName);
            trace.frames[trace.frame_count].line = line.LineNumber;
            ++trace.frame_count;
        }
    }

    SymCleanup(process);
    return trace;
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
    u32 highest_bit = 0;
    if(!_BitScanForward64(&highest_bit, value))
        return 0; // BitScanForward returns 0 if the value is 0
    return highest_bit;
}

u64 os_lowest_bit_set(u64 value) {
    u32 lowest_bit = 0;
    if(!_BitScanReverse64(&lowest_bit, value))
        return 0; // BitScanReverse returns 0 if the value is 0
    return lowest_bit;
}
