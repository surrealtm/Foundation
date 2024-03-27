#pragma once

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

#define PRINT_STRING(__string) (u32) __string.count, __string.data

struct Allocator;

struct string {
	s64 count;
	u8 *data;

	u8 operator[](s64 index) { assert(index >= 0 && index < this->count); return this->data[index]; }
};

string operator "" _s(const char *literal, size_t size);

s64 cstring_length(char *cstring);
s64 cstring_length(const char *cstring);
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


/* A string builder is a helper struct to create one continuous string from multiple data by
 * concatenating the different information. The string builder returns one string by concatenating
 * the different substrings together.
 * The string builder maintains an internal linked list of different string blocks in case the
 * underlying allocator does not provide it with a continuous block of memory, so that these parts
 * can then be stitched together at the end with as few allocations in-bewteen as possible. */
enum Radix {
	RADIX_floating_point = 0,
	RADIX_binary = 2,
	RADIX_decimal = 10,
	RADIX_hexadecimal = 16,
};

struct String_Builder_Format {
	Radix radix;
	u64 value;
	u8  digits; // For binary, decimal, hexadecimal: Number of digits to print out in total. For floating point: Number of decimal digit.
	u8  fractionals; // Only for floating point: Number of fractional digits.
	b8  sign;
	b8  prefix;
};

struct String_Builder {
	struct Block {
		Block *next;
		u8 *data;
		s64 count;
	};

	Allocator *allocator;
	Block first;
	Block *current;
	s64 total_count;

	u8 *grow(s64 count);
	u64 radix_value(Radix radix, u64 index);
	u64 number_of_required_digits(Radix radix, u64 value);
	u64 number_of_required_digits(Radix radix, s64 value);
	u64 number_of_required_digits(f64 value);
	void append_string_builder_format(String_Builder_Format format);
	void append_digit(u64 digit);

	void create(Allocator *allocator);

	void append(const char *s);
	void append(char *s);
	void append(string s);
	void append(char c);
	void append(s64 v);
	void append(s32 v);
	void append(s16 v);
	void append(s8 v);
	void append(u64 v);
	void append(u32 v);
	void append(u16 v);
	void append(u8 v);
	void append(f64 v);
	void append(f32 v);
	
	string finish();
};