#include "foundation.h"
#include "os_specific.h"
#include "memutils.h"

int main() {
	Memory_Arena arena;
	arena.create(1ULL * ONE_GIGABYTE);

	Memory_Pool pool;
	pool.create(&arena);

	u64 mark = arena.mark();

	int *data = (int *) arena.push(sizeof(int));
	*data = 5;

	int *more_data = (int *) arena.push(sizeof(int));
	*more_data = 2;

	arena.release_from_mark(mark);

	int *the_data = (int *) arena.push(sizeof(int));

	int copy = *data;

	arena.debugPrint();
	pool.debugPrint();

	pool.destroy();
	arena.destroy();
	return copy;
}