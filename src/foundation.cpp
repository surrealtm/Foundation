#include "foundation.h"
#include "os_specific.h"
#include "memutils.h"
#include "string.h"

int main() {
	Linked_List<int> list;
	list.add(1);
	list.add(5);
	list.add(-2);

	int total = 0;

	for(int i : list) {
		total += i;
	}

	return total;
}