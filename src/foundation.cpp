#include "foundation.h"
#include "os_specific.h"
#include "memutils.h"

int main() {
	Memory_Arena arena;
	arena.create(1ULL * ONE_GIGABYTE);

	Memory_Pool pool;
	pool.create(&arena);

	int *a = (int *) pool.push(sizeof(int));
	int *b = (int *) pool.push(sizeof(int));
	int *c = (int *) pool.push(sizeof(int) * 2);

	arena.push(5);

	int *d = (int *) pool.push(sizeof(int) * 4);

	pool.release(c);
	pool.release(d);
	pool.release(b);
	pool.release(a);

	pool.debug_print();

	pool.destroy();
	arena.destroy();
	return 0;
}