#include "string.h"
#include "memutils.h"

string operator "" _s(const char *literal, size_t size) {
	string _string;
	_string.count = size;
	_string.data  = (u8 *) literal;
	return _string;
}

s64 cstring_length(char *cstring) {
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

void String_Builder::create(Allocator *allocator) {
	this->allocator   = allocator;
	this->first       = { 0 };
	this->current     = null;
	this->total_count = 0;
}

void String_Builder::append_string(string s) {
	u8 *pointer = this->grow(s.count);
	memcpy(pointer, s.data, s.count);
}

void String_Builder::append_character(char c) {
	u8 *pointer = this->grow(1);
	*pointer = c;
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
