#include "foundation.h"
#include "os_specific.h"

#define WIN32_MEAN_AND_LEAN
#include <Windows.h>

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


void os_write_to_console(const char *format, ...) {
    va_list args;
    va_start(args, format);
    vprintf(format, args);
    va_end(args);

	printf("\n");
}


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
		report_error("Failed to query committed region size: %s.", error);
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
		report_error("Failed to reserve %" PRIu64 " bytes of memory: %s.", reserved_size, error);
		win32_free_last_error_string(error);
	}

	return base;
}

void os_free_memory(void *base, u64 reserved_size) {
	assert(base != null);
	assert(reserved_size != 0);

	bool result = VirtualFree(base, 0, MEM_RELEASE);

	if(!result) {
		char *error = win32_last_error_to_string();
		report_error("Failed to free %" PRIu64 " bytes of memory: %s.", reserved_size, error);
		win32_free_last_error_string(error);
	}
}

bool os_commit_memory(void *address, u64 commit_size) {
	assert(address != null);
	assert(commit_size != 0);

	void *result = VirtualAlloc(address, commit_size, MEM_COMMIT, PAGE_READWRITE);

	if(!result) {
		char *error = win32_last_error_to_string();
		report_error("Failed to commit %" PRIu64 " bytes of memory: %s.", commit_size, error);
		win32_free_last_error_string(error);
	}

	return result != null;
}

void os_decommit_memory(void *address, u64 decommit_size) {
	assert(address != null);
	assert(decommit_size != 0);

	bool result = VirtualFree(address, decommit_size, MEM_DECOMMIT); // This should just decommit parts of the virtual address space, we'll free it later in os_free_memory.

	if(!result) {
		char *error = win32_last_error_to_string();
		report_error("Failed to decommit %" PRIu64 " bytes of memory: %s.", decommit_size, error);
		win32_free_last_error_string(error);
	}
}



u64 os_highest_bit_set(u64 value) {
    u32 highest_bit = 0;
    if(!_BitScanForward64(&highest_bit, value))
        return 0; // BitScanForward returns 0 if the value is 0
    return highest_bit;
};


u64 os_lowest_bit_set(u64 value) {
    u32 lowest_bit = 0;
    if(!_BitScanReverse64(&lowest_bit, value))
        return 0; // BitScanReverse returns 0 if the value is 0
    return lowest_bit;
}