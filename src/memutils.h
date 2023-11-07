#pragma once

#include "foundation.h"

#define ONE_GIGABYTE (ONE_MEGABYTE * ONE_MEGABYTE)
#define ONE_MEGABYTE (ONE_KILOBYTE * ONE_KILOBYTE)
#define ONE_KILOBYTE (1024ULL)
#define ONE_BYTE     (1ULL)

struct Memory_Arena {
	void *base      = null;
	u64 commit_size = 0;
	u64 page_size   = 0;
	u64 committed   = 0;
	u64 reserved    = 0;
	u64 size        = 0;
	
	void create(u64 reserved, u64 requested_commit_size);
	void destroy();

	void *push(u64 size);
};