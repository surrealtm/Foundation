#include "foundation.h"
#include "os_specific.h"
#include "memutils.h"

int main() {
	Memory_Arena arena;
	arena.create(1ULL * ONE_GIGABYTE);

	Memory_Pool pool;
	pool.create(&arena);

	Allocator allocator = pool.allocator();

	int *a = (int *) allocator.allocate(sizeof(int));
	int *b = (int *) allocator.allocate(sizeof(int));
	int *c = (int *) allocator.allocate(sizeof(int) * 2);

	allocator.allocate(5);

	int *d = (int *) allocator.allocate(sizeof(int) * 4);

	allocator.deallocate(c);
	allocator.deallocate(d);
	allocator.deallocate(b);
	allocator.deallocate(a);

	pool.debug_print();

	allocator.debug_print(0);

	pool.destroy();
	arena.destroy();
	return 0;
}