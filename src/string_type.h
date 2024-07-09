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

	u8 &operator[](s64 index) { assert(index >= 0 && index < this->count); return this->data[index]; }
};



/* ------------------------------------------------ Characters ------------------------------------------------ */

b8 is_lower_character(u8 c);
b8 is_upper_character(u8 c);
u8 to_lower_character(u8 c);
u8 to_upper_character(u8 c);



/* ------------------------------------------------ C Strings ------------------------------------------------ */

s64 cstring_length(const char *cstring);
s64 cstring_length(char *cstring);
string from_cstring(Allocator *allocator, char *cstring);
string cstring_view(const char *cstring);
char *to_cstring(Allocator *allocator, string _string);
void free_cstring(Allocator *allocator, char *cstring);

s64 search_cstring(const char *string, u8 _char);
s64 search_cstring_reverse(const char *string, u8 _char);

b8 cstrings_equal(const char *lhs, const char *rhs);
b8 cstrings_equal(const char *lhs, const char *rhs, s64 length);
b8 cstrings_equal_ignore_case(const char *lhs, const char *rhs);
b8 cstrings_equal_ignore_case(const char *lhs, const char *rhs, s64 length);
b8 cstring_starts_with(const char *lhs, const char *rhs);
b8 cstring_starts_with_ignore_case(const char *lhs, const char *rhs);
b8 cstring_ends_with(const char *lhs, const char *rhs);
b8 cstring_ends_with_ignore_case(const char *lhs, const char *rhs);

void copy_cstring(char *dst, const char *src);
void copy_cstring(char *dst, const char *src, s64 src_count);
void copy_cstring(char *dst, s64 dst_count, const char *src, s64 src_count);


/* ------------------------------------------------- Strings ------------------------------------------------- */

string operator "" _s(const char *literal, size_t size);
string strltr(char *literal); // Build a string from a string literal
string strltr(const char *literal); // Build a string from a string literal
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

b8 strings_equal(const string &lhs, const string &rhs);
b8 string_starts_with(string lhs, string rhs);
b8 string_ends_with(string lhs, string rhs);

string read_next_line(string *input);

u64 string_hash(const string &input);
u64 string_hash(const char *input);



/* -------------------------------------------- String Conversion -------------------------------------------- */

s64 string_to_int(string input, b8 *success);
f64 string_to_double(string input, b8 *success);
f32 string_to_float(string input, b8 *success);
b8 string_to_bool(string input, b8 *success);



/* ---------------------------------------------- String Builder ---------------------------------------------- */

/* A string builder is a helper struct to create one continuous string from multiple data by
 * concatenating the different information. The string builder returns one string by concatenating
 * the different substrings together.
 * The string builder maintains an internal linked list of different string blocks in case the
 * underlying allocator does not provide it with a continuous block of memory, so that these parts
 * can then be stitched together at the end with as few allocations in-bewteen as possible. */

struct String_Builder_Format {
	Radix radix;
	u64 value;
	u8  digits; // For binary, decimal, hexadecimal: Number of digits to print out in total. For floating point: Number of decimal digit.
	u8  fractionals; // Only for floating point: Number of fractional digits.
	b8  _signed;
	b8  prefix;

	String_Builder_Format(Radix radix, u64 value, u8 digits = MAX_U8, u8 fractionals = MAX_U8, b8 _signed = false, b8 prefix = true) :
		radix(radix), value(value), digits(digits), fractionals(fractionals), _signed(_signed), prefix(prefix) {};
	String_Builder_Format(Radix radix, s64 value, u8 digits = MAX_U8, u8 fractionals = MAX_U8, b8 _signed = true, b8 prefix = true) :
		radix(radix), value(value), digits(digits), fractionals(fractionals), _signed(_signed), prefix(prefix) {};
};

struct String_Builder {
	const static s64 BLOCK_SIZE = 256;
	
	struct Block {
		Block *next;
		u8 data[BLOCK_SIZE];
		s64 count;
	};

	Allocator *allocator = null;
	Block first;
	Block *current = null;
	s64 total_count = 0;

	u8 *grow(s64 count);
	u64 radix_value(Radix radix, u64 index);
	u64 number_of_required_digits(Radix radix, u64 value);
	u64 number_of_required_digits(Radix radix, s64 value);
	u64 number_of_required_digits(f64 value);
	void append_string_builder_format(String_Builder_Format format);
	void append_digit(u64 digit);

	void create(Allocator *allocator);
	void destroy(); // This destroys all underlying data of the string builder. This might just pull the rug under the finished()'ed string!

	void append_u8(u8 v);
	void append_u16(u16 v);
	void append_u32(u32 v);
	void append_u64(u64 v);
	void append_s8(s8 v);
	void append_s16(s16 v);
	void append_s32(s32 v);
	void append_s64(s64 v);
	void append_f32(f32 v);
	void append_f64(f64 v);
	void append_char(char c);
	void append_string(const char *s);
	void append_string(char *s);
	void append_string(string s);
	
	string finish();
};

string mprint(Allocator *allocator, const char *format, ...);
