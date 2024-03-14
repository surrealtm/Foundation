#include "foundation.h"

/*
 * Many APIs in this code base prefer a custom string type over the built-in c-string
 * data type. This custom string type stores the length explicitely instead of using
 * an implicit null terminator, which has various benefits:
 *   1. We don't ever need to watch out for a null-terminator, we know the length
        of the string, improving SIMD operations.
     2. There is never a need to calculate the string length, we can just read it.
	    This is useful for serialization, string manipulation, and many other things.
     3. A string may contain '0' bytes, e.g. when representing binary data.
	 4. We can have string views over bigger strings, files, whatever, without
	    needing to copy the data. This is a massive performance improvement.
*/

struct Allocator;

struct string {
	s64 count;
	u8 *data;

	u8 operator[](s64 index) { assert(index >= 0 && index < this->count); return this->data[index]; }
};

string operator "" _Z(const char *literal, size_t size);

s64 cstring_length(char *cstring);
string from_cstring(Allocator *allocator, char *cstring);
char *to_cstring(Allocator *allocator, string _string);

string strltr(char *literal); // Build a string from a string literal
string string_view(u8 *data, s64 count);
string make_string(Allocator *allocator, u8 *data, s64 count);
string allocate_string(Allocator *allocator, s64 count);
void deallocate_string(Allocator *allocator, string *_string);

string copy_string(Allocator *allocator, string input);
string substring(Allocator *allocator, string input, s64 start, s64 end);
string substring_view(string input, s64 start, s64 end);

string concatenate_strings(Allocator *allocator, string lhs, string rhs);

s64 search_string(string _string, u8 _char); // Returns -1 if the character is not found.
s64 search_string_reverse(string _string, u8 _char);

b8 compare_strings(string lhs, string rhs);
b8 string_starts_with(string lhs, string rhs);
b8 string_ends_with(string lhs, string rhs);