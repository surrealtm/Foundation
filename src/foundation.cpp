#include "foundation.h"
#include "os_specific.h"
#include "memutils.h"
#include "string.h"

int main() {
	string s = "Hello World"_Z;

	Linked_List<string> list;
	list.add(s);

	return 0;
}