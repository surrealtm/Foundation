#include "string.h"
#include "memutils.h"
#include "os_specific.h"

string operator "" _s(const char *literal, size_t size) {
	string _string;
	_string.count = size;
	_string.data  = (u8 *) literal;
	return _string;
}



/* ------------------------------------------------- C String ------------------------------------------------- */

s64 cstring_length(char *cstring) {
	s64 length = 0;
	while(*cstring) {
		++length;
		++cstring;
	}
	return length;
}

s64 cstring_length(const char *cstring) {
	s64 length = 0;
	while(*cstring) {
		++length;
		++cstring;
	}
	return length;
}

string from_cstring(Allocator *allocator, char *cstring) {
	string _string;
	_string.count = cstring_length(cstring);
	_string.data  = (u8 *) allocator->allocate(_string.count);
	memcpy(_string.data, cstring, _string.count);
	return _string;
}

char *to_cstring(Allocator *allocator, string _string) {
	char *cstring = (char *) allocator->allocate(_string.count);
	memcpy(cstring, _string.data, _string.count);
	return cstring;
}

void free_cstring(Allocator *allocator, char *cstring) {
	allocator->deallocate(cstring);
}



/* -------------------------------------------------- String -------------------------------------------------- */

string strltr(char *literal) {
	string _string;
	_string.count = cstring_length(literal);
	_string.data  = (u8 *) literal;
	return _string;
}

string string_view(u8 *data, s64 count) {
	string _string;
	_string.count = count;
	_string.data  = data;
	return _string;
}

string make_string(Allocator *allocator, u8 *data, s64 count) {
	string _string;
	_string.count = count;
	_string.data  = (u8 *) allocator->allocate(count);
	memcpy(_string.data, data, _string.count);
	return _string;
}

string allocate_string(Allocator *allocator, s64 count) {
	string _string;
	_string.count = count;
	_string.data  = (u8 *) allocator->allocate(_string.count);
	return _string;
}

void deallocate_string(Allocator *allocator, string *_string) {
	allocator->deallocate(_string->data);
	_string->count = 0;
	_string->data  = null;
}


string copy_string(Allocator *allocator, string input) {
	string output;
	output.count = input.count;
	output.data  = (u8 *) allocator->allocate(output.count);
	memcpy(output.data, input.data, input.count);
	return output;
}

string substring(Allocator *allocator, string input, s64 start, s64 end) {
	assert(start >= 0 && start < input.count && end >= 0 && end < input.count);
	string output = allocate_string(allocator, end - start);
	memcpy(output.data, input.data + start, output.count);
	return output;
}

string substring_view(string input, s64 start, s64 end) {
	assert(start >= 0 && start < input.count && end >= 0 && end < input.count);
	string output;
	output.count = end - start;
	output.data  = input.data + start;
	return output;
}


string concatenate_strings(Allocator *allocator, string lhs, string rhs) {
	string output;
	output.count = lhs.count + rhs.count;
	output.data  = (u8 *) allocator->allocate(output.count);
	memcpy(&output.data[0], lhs.data, lhs.count);
	memcpy(&output.data[lhs.count], rhs.data, rhs.count);
	return output;
}


s64 search_string(string _string, u8 _char) {
	for(s64 i = 0; i < _string.count; ++i) {
		if(_string.data[i] == _char) return i;
	}

	return -1;
}

s64 search_string_reverse(string _string, u8 _char) {
	for(s64 i = _string.count - 1; i >= 0; --i) {
		if(_string.data[i] == _char) return i;
	}

	return -1;
}


b8 compare_strings(string lhs, string rhs) {
	if(lhs.count != rhs.count) return false;

	return memcmp(lhs.data, rhs.data, lhs.count);
}

b8 string_starts_with(string lhs, string rhs) {
	if(rhs.count > lhs.count) return false;

	return compare_strings(substring_view(lhs, 0, rhs.count), rhs);
}

b8 string_ends_with(string lhs, string rhs) {
	if(rhs.count > lhs.count) return false;

	return compare_strings(substring_view(lhs, lhs.count - rhs.count, lhs.count), rhs);
}



/* ---------------------------------------------- String Builder ---------------------------------------------- */

u8 *String_Builder::grow(s64 count) {
	u8 *next_content_block = (u8 *) this->allocator->allocate(count);

	if(!this->current) {
		// This is the first time the string builder has been used. Set up the
		// first node without an allocation to provide a fast code path for small
		// strings.
		this->current = &this->first;
		this->current->next = null;
		this->current->data = next_content_block;
		this->current->count = 0;
	}

	if(this->current->data + this->current->count != next_content_block) {
	    // The allocator could not provide us with continous data for the current block,
        // so we need to add another block entry into our list entry.
        // We don't use the provided allocator for these blocks since that would mean we
        // can never continously allocate (since the block entry comes between the
        // current content, and content in the future...)
		Block *block = (Block *) Default_Allocator->allocate(sizeof(Block));
		block->next  = null;
		block->count = count;
		block->data  = next_content_block;
		this->current->next = block;
		this->current       = block;
	} else {
		// The next content block is continuous to the previous block, so we can just
		// append the new data to the block.
		this->current->count += count;
	}

	this->total_count += count;
	
	return next_content_block;
}

u64 String_Builder::radix_value(Radix radix, u64 index) {
	u64 power = 1;

	switch(radix) {
	case RADIX_binary:      power = 1ULL << index; break;
	case RADIX_hexadecimal: power = 1ULL << (index * 4); break;
	case RADIX_decimal:
		for(u64 i = 0; i < index; ++i) {
			power *= 10;
		}
		break;
	}

	return power;
}

u64 String_Builder::number_of_required_digits(Radix radix, u64 value) {
	if(value == 0) return 1;
	
	u64 count = 0;

	while(value != 0) {
		value /= radix;
		++count;
	}

	return count;
}

u64 String_Builder::number_of_required_digits(Radix radix, s64 value) {
	if(value == 0) return 1;
	
	u64 count = 0;

	while(value != 0) {
		value /= radix;
		++count;
	}

	return count;
}

u64 String_Builder::number_of_required_digits(f64 value) {
	if(value == 0) return 1;

	u64 count = 0;

	while(value >= 1 || value <= -1) {
		value /= 10;
		++count;
	}

	return count;
}

void String_Builder::append_string_builder_format(String_Builder_Format format) {
	if(format.prefix) {
		switch(format.radix) {
		case RADIX_binary: this->append("0b"_s); break;
		case RADIX_hexadecimal: this->append("0x"_s); break;
		}
	}
	
	if(format.radix == RADIX_binary || format.radix == RADIX_hexadecimal || (format.radix == RADIX_decimal && !format.sign)) {
		u64 value = format.value;
		u64 required_digits = this->number_of_required_digits(format.radix, value);
		
		//
		// Print out leading zeros.
		//
		for(u64 i = required_digits; i < format.digits; ++i) {
			this->append('0');
		}
		
		//
		// Print out the actual value.
		//
		s64 index = required_digits - 1;
		
		while(index >= 0) {
			u64 power = this->radix_value(format.radix, index);
			u64 digit = value / power;
			
			this->append_digit(digit);

			value -= digit * power;
			--index;
		}
	} else if(format.radix == RADIX_decimal && format.sign) {
		u64 value = format.value;
		u64 required_digits = this->number_of_required_digits(format.radix, (s64) value);
		
		//
		// Handle sign.
		//
		if(value & 0x8000000000000000) { // value < 0
			value = (u64) (- (s64) value);
			this->append('-');
		} else if(format.prefix) {
			this->append('+');
		}
		
		//
		// Print out leading zeros.
		//
		for(u64 i = required_digits; i < format.digits; ++i) {
			this->append('0');
		}

		//
		// Print out the actual value.
		//
		s64 index = required_digits - 1;
		
		while(index >= 0) {
			u64 power = this->radix_value(format.radix, index);
			u64 digit = value / power;
			
			this->append_digit(digit);

			value -= digit * power;
			--index;
		}
	} else if(format.radix == RADIX_floating_point) {
		f64 value;
		memcpy(&value, &format.value, sizeof(f64));
		u64 required_digits = this->number_of_required_digits(value);

		//
		// Handle sign.
		//
		if(value < 0) {
			value = -value;
			this->append('-');
		} else if(format.prefix) {
			this->append('+');
		}

		//
		// Print out leading zeros.
		//
		for(u64 i = required_digits; i < format.digits; ++i) {
			this->append('0');
		}

		//
		// Print out the decimal spaces.
		//
		s64 index = required_digits - 1;

		while(index >= 0) {
			f64 power = pow(10, (f64) index);
			f64 digit = floor(value / power);

			this->append_digit((u64) digit);

			value -= digit * power;
			--index;
		}

		//
		// Print the decimal point.
		//

		this->append('.');

		//
		// Print the fractional spaces.
		//
		for(u64 i = 0; i < format.fractionals; ++i) {
			value *= 10;

			f64 digit = floor(value);
		
			this->append_digit((u64) digit);
		
			value -= digit;
		}
	}
}

void String_Builder::append_digit(u64 value) {
	if(value >= 0 && value <= 9) {
		this->append((char) (value + '0'));
	} else if(value >= 10 && value <= 15) {
		this->append((char) (value - 10 + 'a'));
	}
}

void String_Builder::create(Allocator *allocator) {
	this->allocator   = allocator;
	this->first       = { 0 };
	this->current     = null;
	this->total_count = 0;
}

void String_Builder::append(const char *s) {
	s64 length = cstring_length(s);
	u8 *pointer = this->grow(length);
	memcpy(pointer, s, length);
}

void String_Builder::append(char *s) {
	s64 length = cstring_length(s);
	u8 *pointer = this->grow(length);
	memcpy(pointer, s, length);
}

void String_Builder::append(string s) {
	u8 *pointer = this->grow(s.count);
	memcpy(pointer, s.data, s.count);
}

void String_Builder::append(char c) {
	u8 *pointer = this->grow(1);
	*pointer = c;
}

void String_Builder::append(s64 v) {
	this->append_string_builder_format({ RADIX_decimal, (u64) (s64) v, 1, 0, true, false });
}

void String_Builder::append(s32 v) {
	this->append_string_builder_format({ RADIX_decimal, (u64) (s64) v, 1, 0, true, false });
}

void String_Builder::append(s16 v) {
	this->append_string_builder_format({ RADIX_decimal, (u64) (s64) v, 1, 0, true, false });
}

void String_Builder::append(s8 v) {
	this->append_string_builder_format({ RADIX_decimal, (u64) (s64) v, 1, 0, true, false });
}

void String_Builder::append(u64 v) {
	this->append_string_builder_format({ RADIX_decimal, (u64) v, 1, 0, false, false });
}

void String_Builder::append(u32 v) {
	this->append_string_builder_format({ RADIX_decimal, (u64) v, 1, 0, false, false });
}

void String_Builder::append(u16 v) {
	this->append_string_builder_format({ RADIX_decimal, (u64) v, 1, 0, false, false });
}

void String_Builder::append(u8 v) {
	this->append_string_builder_format({ RADIX_decimal, (u64) v, 1, 0, false, false });
}

void String_Builder::append(f64 value) {
	u64 _u64;
	memcpy(&_u64, &value, sizeof(f64));
	this->append_string_builder_format({ RADIX_floating_point, _u64, 1, 5, true, false });
}

void String_Builder::append(f32 value) {
	this->append((f64) value);
}

string String_Builder::finish() {
	if(this->first.next) {
		// There are at least two different blocks of data which we need to concatenate here.
		string result = allocate_string(this->allocator, this->total_count);
		s64 offset = 0;

		Block *block = &this->first;
		while(block) {
			memcpy(&result.data[offset], block->data, block->count);
			offset += block->count;

			this->allocator->deallocate(block->data);
			if(block != &this->first) Default_Allocator->deallocate(block);

			block = block->next;
		}
	
		return result;
	} else {
		// There was only ever one block of data, meaning this block is already our complete
		// string.
		return string_view(this->first.data, this->first.count);
	}
}
