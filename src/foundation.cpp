#include "foundation.h"
#include "os_specific.h"
#include "memutils.h"
#include "string.h"
#include "csp.h"

int main() {
	String_Builder builder;
	builder.create(Default_Allocator);
	builder.append_string("Hello World: "_s);
	builder.append_character('A');
	builder.append_string("!"_s);
	string result = builder.finish();

	printf("Result: %.*s\n", PRINT_STRING(result));

	return 0;
}