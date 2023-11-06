#include "foundation.h"
#include "os_specific.h"
#include "memutils.h"

int main() {
	Memory_Arena arena;
	arena.create(1ULL * ONE_GIGABYTE, 4096);

	int *data = (int *) arena.push(sizeof(int));
	*data = 5;

	arena.destroy();
	return *data;
}