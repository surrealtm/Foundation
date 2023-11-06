#pragma once

#include "foundation.h"

void os_write_to_console(const char *format, ...);

void *os_reserve_memory(u64 reserved_size);
void os_free_memory(void *base, u64 reserved_size);
void os_commit_memory(void *base, u64 commit_size);
void os_decommit_memory(void *base, u64 decommit_size);