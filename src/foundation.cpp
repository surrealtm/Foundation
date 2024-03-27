#include "foundation.h"
#include "os_specific.h"
#include "memutils.h"
#include "string.h"

int main() {
	String_Builder builder;
	builder.create(Default_Allocator);

	u64 x = MAX_U64;

	{
		builder.append("Unsigned integers: "_s);
		builder.append(MAX_U64);
		builder.append(" | ");
		builder.append(MAX_U32);
		builder.append(" | ");
		builder.append(MAX_U16);
		builder.append(" | ");
		builder.append(MAX_U8);
		builder.append("\n                 "_s);
		builder.append((u64) 543543543);
		builder.append(" | ");
		builder.append((u32) 12345);
		builder.append(" | ");
		builder.append((u16) 100);
		builder.append(" | ");
		builder.append((u8) 127);
	}

	builder.append("\n");

	{
		builder.append("Signed integers: "_s);
		builder.append(MAX_S64);
		builder.append(" | ");
		builder.append(MAX_S32);
		builder.append(" | ");
		builder.append(MAX_S16);
		builder.append(" | ");
		builder.append(MAX_S8);
		builder.append("\n                 "_s);
		builder.append(MIN_S64);
		builder.append(" | ");
		builder.append(MIN_S32);
		builder.append(" | ");
		builder.append(MIN_S16);
		builder.append(" | ");
		builder.append(MIN_S8);
		builder.append("\n                 "_s);
		builder.append((s64) -12345678);
		builder.append(" | ");
		builder.append((s32) -53);
		builder.append(" | ");
		builder.append((s16) 1);
		builder.append(" | ");
		builder.append((s8) 100);
	}
	
	builder.append("\n");

	{
		builder.append("Doubles: "_s);
		builder.append(MAX_F64);
		builder.append(" | ");
		builder.append(MIN_F64);
		builder.append(" | ");
		builder.append(-123456.789);
	}

	string result = builder.finish();

	printf("%.*s\n", PRINT_STRING(result));

	return 0;
}