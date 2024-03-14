#include "foundation.h"
#include "os_specific.h"
#include "memutils.h"
#include "string.h"

int main() {
	string s = "Hello World"_Z;

	Resizable_Array<string> array;
	array.add(s);

	return 0;
}