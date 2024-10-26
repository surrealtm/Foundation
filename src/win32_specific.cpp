#include "os_specific.h"
#include "memutils.h"

#define WIN32_MEAN_AND_LEAN
#include <wchar.h> // Apparently some fucking windows header requires this somethimes or something I don't even know I don't want to have to deal with this shit.
#include <Windows.h>
#include <psapi.h> // For getting memory usage
#include <dbghelp.h> // For stack walking
#include <shellscalingapi.h> // For DPI awareness.
#include <audioclient.h>



/* ---------------------------------------------- Win32 Helpers ---------------------------------------------- */

const char *win32_hresult_to_string(s64 hresult) {
    const char *output;

    switch(hresult) {
    case S_OK:                               output = "S_OK"; break;
    case REGDB_E_CLASSNOTREG:                output = "REGDB_E_CLASSNOTREG"; break;
    case REGDB_E_IIDNOTREG:                  output = "REGDB_E_IIDNOTREG"; break;
    case CLASS_E_NOAGGREGATION:              output = "CLASS_E_NOAGGREGATION"; break;
    case E_NOINTERFACE:                      output = "E_NOINTERFACE"; break;
    case E_POINTER:                          output = "E_POINTER"; break;
    case CO_E_NOTINITIALIZED:                output = "CO_E_NOTINITIALIZED"; break;
    case AUDCLNT_S_BUFFER_EMPTY:             output = "AUDCLNT_S_BUFFER_EMPTY"; break;
    case AUDCLNT_E_NOT_INITIALIZED:          output = "AUDCLNT_E_NOT_INITIALIZED"; break;
    case AUDCLNT_E_WRONG_ENDPOINT_TYPE:      output = "AUDCLNT_E_WRONG_ENDPOINT_TYPE"; break;
    case AUDCLNT_E_DEVICE_INVALIDATED:       output = "AUDCLNT_E_DEVICE_INVALIDATED"; break;
    case AUDCLNT_E_SERVICE_NOT_RUNNING:      output = "AUDCLNT_E_SERVICE_NOT_RUNNING"; break;
    case AUDCLNT_E_BUFFER_ERROR:             output = "AUDCLNT_E_BUFFER_ERROR"; break;
    case AUDCLNT_E_OUT_OF_ORDER:             output = "AUDCLNT_E_OUT_OF_ORDER"; break;
    case AUDCLNT_E_BUFFER_OPERATION_PENDING: output = "AUDCLNT_E_BUFFER_OPERATION_PENDING"; break;
    default:                                 output = "UNKNOWN_HRESULT"; break;
    }
    
    return output;
}

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

void os_message_box(string message) {
    ShowCursor(true); // Show the cursor in case we have hidden it with the window module.
    
    char *cstring = to_cstring(Default_Allocator, message);
    defer { free_cstring(Default_Allocator, cstring); };

    DWORD type = MB_OK | MB_ICONERROR;
    MessageBoxA(null, cstring, "Foundation | Assertion Failed.", type);
}

void os_debug_break() {
    DebugBreak();
}

void os_terminate_process(u32 exit_code) {
    ExitProcess(exit_code);
}

void os_enable_high_resolution_clock() {
    timeBeginPeriod(1);
}

void os_get_desktop_dpi(s32 *x, s32 *y) {
    UINT uintx, uinty;
    HMONITOR desktop_monitor = MonitorFromWindow(GetDesktopWindow(), MONITOR_DEFAULTTOPRIMARY);
    GetDpiForMonitor(desktop_monitor, MDT_EFFECTIVE_DPI, &uintx, &uinty);
    *x = (s32) uintx;
    *y = (s32) uinty;
}

b8 os_load_and_run_dynamic_library(string file_path, string procedure, void *argument) {
    char *file_path_cstring = to_cstring(Default_Allocator, file_path);
    defer { free_cstring(Default_Allocator, file_path_cstring); };

    HINSTANCE dll = LoadLibraryA(file_path_cstring);
    if(!dll) return false;

    char *procedure_cstring = to_cstring(Default_Allocator, procedure);
    defer { free_cstring(Default_Allocator, procedure_cstring); };
    
    INT_PTR(*procedure_pointer)(void *) = (INT_PTR(*)(void *)) GetProcAddress(dll, procedure_cstring);
    if(procedure_pointer) procedure_pointer(argument);
    
    FreeLibrary(dll);
    
    return procedure_pointer != null;
}

b8 os_can_access_pointer(void *pointer) {
    MEMORY_BASIC_INFORMATION mbi = { 0 };

    bool bad_pointer = true;

    if(VirtualQuery(pointer, &mbi, sizeof(mbi))) bad_pointer = (mbi.Protect & (PAGE_GUARD | PAGE_NOACCESS)) != 0;

    return !bad_pointer;
}

s32 os_get_number_of_hardware_threads() {
    SYSTEM_INFO system_info;
    GetSystemInfo(&system_info);
    return system_info.dwNumberOfProcessors;
}

string os_get_user_name() {
    char buffer[256];
    DWORD buffer_size = sizeof(buffer);
    GetUserNameA(buffer, &buffer_size);
    return make_string(Default_Allocator, (u8 *) buffer, buffer_size - 1); // buffer_size includes the null terminator.
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

b8 os_commit_memory(void *address, u64 commit_size, b8 executable) {
	assert(address != null);
	assert(commit_size != 0);

	void *result = VirtualAlloc(address, commit_size, MEM_COMMIT, executable ? PAGE_EXECUTE_READWRITE : PAGE_READWRITE);

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
        
    char *cstring = to_cstring(Default_Allocator, file_path);
    b8 result = CreateDirectoryA(cstring, null);
    free_cstring(Default_Allocator, cstring);

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

File_Information os_get_file_information(string file_path) {
	char *cstring = to_cstring(Default_Allocator, file_path);

    File_Information result = { 0 };

    WIN32_FILE_ATTRIBUTE_DATA win32_data;
    if(GetFileAttributesExA(cstring, GetFileExInfoStandard, &win32_data)) {
        result.valid                  = true;
        result.file_size_in_bytes     = ((u64) win32_data.nFileSizeHigh << 32) | (u64) win32_data.nFileSizeLow;
        result.creation_time          = ((u64) win32_data.ftCreationTime.dwHighDateTime << 32) | (u64) win32_data.ftCreationTime.dwLowDateTime;
        result.last_access_time       = ((u64) win32_data.ftLastAccessTime.dwHighDateTime << 32) | (u64) win32_data.ftLastAccessTime.dwLowDateTime;
        result.last_modification_time = ((u64) win32_data.ftLastWriteTime.dwHighDateTime << 32) | (u64) win32_data.ftLastWriteTime.dwLowDateTime;
    } else {
        result.valid = false;
    }
    
	free_cstring(Default_Allocator, cstring);
    return result;
}



/* ------------------------------------------------ File Paths ------------------------------------------------ */

b8 os_looks_like_absolute_file_path(string file_path) {
	return file_path.count > 2 && file_path[1] == ':' && (file_path[2] == '/' || file_path[2] == '\\');
}

string os_convert_to_absolute_file_path(Allocator *allocator, string file_path) {
    char *cstring = to_cstring(Default_Allocator, file_path);
	u32 buffer_size = GetFullPathNameA(cstring, 0, null, null);
    string result = allocate_string(allocator, buffer_size - 1); // buffer_size includes the null terminator
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



static
void internal_get_files_in_folder(string file_path, Resizable_Array<string> *files, Files_In_Folder_Flags flags) {
    string concatenation = concatenate_strings(Default_Allocator, file_path, "\\*"_s); // The win32 requires this "search pattern" to list all the files in the given folder path...
    defer { deallocate_string(Default_Allocator, &concatenation); };
    
    char *cstring = to_cstring(Default_Allocator, concatenation);
    defer { free_cstring(Default_Allocator, cstring); };

    WIN32_FIND_DATAA find_data;
    HANDLE find_handle = FindFirstFileA(cstring, &find_data);
    
    while(find_handle != INVALID_HANDLE_VALUE) {
        string file_name_view = string_view((u8 *) find_data.cFileName, cstring_length(find_data.cFileName));

        if(find_data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY && !strings_equal(file_name_view, "."_s) && !strings_equal(file_name_view, ".."_s)) {
            if(flags & FILES_IN_FOLDER_Files_And_Folders) {
                if(flags & FILES_IN_FOLDER_Put_Original_Path_Into_Output_Paths) {
                    String_Builder builder;
                    builder.create(files->allocator);
                    builder.append_string(file_path);
                    builder.append_string("\\");
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
                builder.append_string("\\");
                builder.append_string(file_name_view);
                string folder_name = builder.finish();
                internal_get_files_in_folder(folder_name, files, flags);
                deallocate_string(Default_Allocator, &folder_name);
            }
        } else if(!(find_data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)) {
            if(flags & FILES_IN_FOLDER_Put_Original_Path_Into_Output_Paths) {
                String_Builder builder;
                builder.create(files->allocator);
                builder.append_string(file_path);
                builder.append_string("\\");
                builder.append_string(file_name_view);
                files->add(builder.finish());
            } else {
                files->add(copy_string(files->allocator, file_name_view));
            }
        }
        
        if(!FindNextFileA(find_handle, &find_data)) break;
    }

    FindClose(find_handle);
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

LARGE_INTEGER __win32_performance_frequency;
b8 __win32_performance_frequency_set = false;

System_Time os_get_system_time() {
    SYSTEMTIME win32;
    GetLocalTime(&win32);

    System_Time system;
    system.year        = win32.wYear;
    system.month       = win32.wMonth;
    system.day         = win32.wDay;
    system.hour        = win32.wHour;
    system.minute      = win32.wMinute;
    system.second      = win32.wSecond;
    system.millisecond = win32.wMilliseconds;
    return system;
}

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



/* ----------------------------------------------- System Calls ----------------------------------------------- */

static
char *win32_system_call_string(const char *executable, const char *arguments[], s64 argument_count) {
    String_Builder builder;
    builder.create(Default_Allocator);
    builder.append_string(executable);
    for(s64 i = 0; i < argument_count; ++i) {
        builder.append_string(" ");
        builder.append_string(arguments[i]);
    }
    return builder.finish_as_cstring();
}

s32 os_system_call(const char *executable, const char *arguments[], s64 argument_count) {
    char *command_line = win32_system_call_string(executable, arguments, argument_count); // @@Leak
    
    PROCESS_INFORMATION  pi = { 0 };
    STARTUPINFOA start_info = { 0 };
    start_info.cb = sizeof(STARTUPINFO);

    if(!CreateProcessA(null, command_line, null, null, true, 0, null, null, &start_info, &pi)) {
        return -1;
    }

    WaitForSingleObject(pi.hProcess, INFINITE);

    DWORD exit_code;
    GetExitCodeProcess(pi.hProcess, &exit_code);

    CloseHandle(pi.hProcess);
    CloseHandle(pi.hThread);
    return exit_code;

}

s32 os_system_call_wide_string(const wchar_t *command_line) {
    PROCESS_INFORMATION  pi = { 0 };
    STARTUPINFOW start_info = { 0 };
    start_info.cb = sizeof(STARTUPINFO);

    if(!CreateProcessW(null, (LPWSTR) command_line, null, null, true, 0, null, null, &start_info, &pi)) {
        return -1;
    }

    WaitForSingleObject(pi.hProcess, INFINITE);

    DWORD exit_code;
    GetExitCodeProcess(pi.hProcess, &exit_code);

    CloseHandle(pi.hProcess);
    CloseHandle(pi.hThread);
    return exit_code;
}



/* ----------------------------------------------- Stack Trace ----------------------------------------------- */

Stack_Trace os_get_stack_trace() {
#if FOUNDATION_DEVELOPER
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
    u32 highest_bit = 0;
    if(!_BitScanReverse64(&highest_bit, value))
        return 0; // BitScanForward returns 0 if the value is 0
    return highest_bit;
}

u64 os_lowest_bit_set(u64 value) {
    u32 lowest_bit = 0;
    if(!_BitScanForward64(&lowest_bit, value))
        return 0; // BitScanReverse returns 0 if the value is 0
    return lowest_bit;
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
