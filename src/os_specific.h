#pragma once

#include "foundation.h"

void os_write_to_console(const char *format, ...);

u64 os_get_page_size();
u64 os_get_committed_region_size(void *base);
void *os_reserve_memory(u64 reserved_size);
void os_free_memory(void *base, u64 reserved_size);
bool os_commit_memory(void *base, u64 commit_size);
void os_decommit_memory(void *base, u64 decommit_size);