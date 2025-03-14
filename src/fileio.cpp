#include "fileio.h"
#include "memutils.h"
#include "os_specific.h"



/* -------------------------------------------- Helper Procedures -------------------------------------------- */

static
b8 is_string_character(u8 c) {
    return (c >= 'a' && c <= 'z') || 
		(c >= 'A' && c <= 'Z') ||
        (c >= '0' && c <= '9') ||
		(c == '_' || c == '\\' || c == '/' || c == '.');
}



/* ----------------------------------------------- Ascii_Parser ----------------------------------------------- */

void Ascii_Parser::create_from_string(string data) {
    this->data = data;
    this->position = 0;
}

void Ascii_Parser::create_from_buffer(u8 *data, s64 size) {
    this->data = string_view(data, size);
    this->position = 0;
}

b8 Ascii_Parser::create_from_file(string file_path) {
    this->data = os_read_file(Default_Allocator, file_path);
    this->position = 0;
	return this->data.count > 0;
}

void Ascii_Parser::destroy_file_data() {
    deallocate_string(Default_Allocator, &this->data);
}

b8 Ascii_Parser::finished() {
	return this->position == this->data.count;
}

string Ascii_Parser::read_string() {
    //
    // Skip empty characters.
    //
    while(this->position < this->data.count && this->data[this->position] >= 0 && this->data[this->position] <= 32) {
        ++this->position;
    }

    //
    // Read in the string.
    //
    if(this->position == this->data.count) return ""_s;

    s64 string_start = this->position;

    while(this->position < this->data.count && is_string_character(this->data[this->position])) {
        ++this->position;
    }

    return string_view(&this->data[string_start], this->position - string_start);
}

u8 Ascii_Parser::read_u8() {
    string _string = this->read_string();
    b8 success;
	u8 value = (u8) string_to_int(_string, &success);
	return value;
}

u16 Ascii_Parser::read_u16() {
    string _string = this->read_string();
    b8 success;
    u16 value = (u16) string_to_int(_string, &success);
    return value;
}

u32 Ascii_Parser::read_u32() {
    string _string = this->read_string();
    b8 success;
    u32 value = (u32) string_to_int(_string, &success);
    return value;
}

u64 Ascii_Parser::read_u64() {
    string _string = this->read_string();
    b8 success;
    u64 value = (u64) string_to_int(_string, &success);
    return value;
}

s8 Ascii_Parser::read_s8() {
    string _string = this->read_string();
    b8 success;
    s8 value = (s8) string_to_int(_string, &success);
    return value;
}

s16 Ascii_Parser::read_s16() {
    string _string = this->read_string();
    b8 success;
    s16 value = (s16) string_to_int(_string, &success);
    return value;
}

s32 Ascii_Parser::read_s32() {
    string _string = this->read_string();
    b8 success;
    s32 value = (s32) string_to_int(_string, &success);
    return value;
}

s64 Ascii_Parser::read_s64() {
    string _string = this->read_string();
    b8 success;
    s64 value = (s64) string_to_int(_string, &success);
    return value;
}

f32 Ascii_Parser::read_f32() {
    string _string = this->read_string();
    b8 success;
    f32 value = string_to_float(_string, &success);
    return value;
}

f64 Ascii_Parser::read_f64() {
    string _string = this->read_string();
    b8 success;
    f64 value = string_to_double(_string, &success);
    return value;
}



/* ----------------------------------------------- Ascii_Writer ----------------------------------------------- */

void Ascii_Writer::create(string file_path, s64 /*buffer_size*/) {
    this->file_path = copy_string(Default_Allocator, file_path);
	this->builder.create(Default_Allocator);
}

void Ascii_Writer::destroy() {
    this->flush();
    deallocate_string(Default_Allocator, &this->file_path);
    this->builder.destroy();
}

void Ascii_Writer::flush() {
    s64 directory_end = os_search_path_for_directory_slash_reverse(this->file_path);
	if(directory_end != -1) os_create_directory(substring_view(this->file_path, 0, directory_end));

	b8 append = false;
	for(String_Builder::Block *block = &this->builder.first; block != null; block = block->next) {
		os_write_file(this->file_path, string_view(block->data, block->count), append);
		append = true;
	}
}

void Ascii_Writer::write_string(string data) {
	this->builder.append_string(data);
}

void Ascii_Writer::write_char(char c) {
    this->builder.append_char(c);
}

void Ascii_Writer::write_u8(u8 value) {
    this->builder.append_u8(value);
}

void Ascii_Writer::write_u16(u16 value) {
    this->builder.append_u16(value);
}

void Ascii_Writer::write_u32(u32 value) {
    this->builder.append_u32(value);
}

void Ascii_Writer::write_u64(u64 value) {
    this->builder.append_u64(value);
}

void Ascii_Writer::write_s8(s8 value) {
    this->builder.append_s8(value);
}

void Ascii_Writer::write_s16(s16 value) {
    this->builder.append_s16(value);
}

void Ascii_Writer::write_s32(s32 value) {
    this->builder.append_s32(value);
}

void Ascii_Writer::write_s64(s64 value) {
    this->builder.append_s64(value);
}

void Ascii_Writer::write_f32(f32 value) {
    this->builder.append_f32(value);
}

void Ascii_Writer::write_f64(f64 value) {
    this->builder.append_f64(value);
}



/* ---------------------------------------------- Binary_Parser ---------------------------------------------- */

void Binary_Parser::create_from_string(string data) {
	this->data = data;
	this->position = 0;
}

void Binary_Parser::create_from_buffer(u8 *data, s64 size) {
	this->data = string_view(data, size);
	this->position = 0;
}

b8 Binary_Parser::create_from_file(string file_path) {
	this->data = os_read_file(Default_Allocator, file_path);
	this->position = 0;
	return this->data.count > 0;
}

void Binary_Parser::destroy_file_data() {
	deallocate_string(Default_Allocator, &this->data);
}

void *Binary_Parser::read(s64 size_in_bytes) {
	assert(this->position + size_in_bytes <= this->data.count);
	void *pointer = &this->data.data[this->position];
	this->position += size_in_bytes;
	return pointer;
}

u8 Binary_Parser::read_u8() {
	u8 *pointer = (u8 *) this->read(sizeof(u8));
	u8 value = *pointer;
	return value;
}

u16 Binary_Parser::read_u16() {
	u16 *pointer = (u16 *) this->read(sizeof(u16));
	u16 value = *pointer;
#if !FOUNDATION_LITTLE_ENDIAN
	byteswap2(&value);
#endif
	return value;
}

u32 Binary_Parser::read_u32() {
	u32 *pointer = (u32 *) this->read(sizeof(u32));
	u32 value = *pointer;
#if !FOUNDATION_LITTLE_ENDIAN
	byteswap4(&value);
#endif
	return value;
}

u64 Binary_Parser::read_u64() {
	u64 *pointer = (u64 *) this->read(sizeof(u64));
	u64 value = *pointer;
#if !FOUNDATION_LITTLE_ENDIAN
	byteswap8(&value);
#endif
	return value;
}

s8 Binary_Parser::read_s8() {
	s8 *pointer = (s8 *) this->read(sizeof(s8));
	s8 value = *pointer;
	return value;
}

s16 Binary_Parser::read_s16() {
	s16 *pointer = (s16 *) this->read(sizeof(s16));
	s16 value = *pointer;
#if !FOUNDATION_LITTLE_ENDIAN
	byteswap2(&value);
#endif
	return value;
}

s32 Binary_Parser::read_s32() {
	s32 *pointer = (s32 *) this->read(sizeof(s32));
	s32 value = *pointer;
#if !FOUNDATION_LITTLE_ENDIAN
	byteswap4(&value);
#endif
	return value;
}

s64 Binary_Parser::read_s64() {
	s64 *pointer = (s64 *) this->read(sizeof(s64));
	s64 value = *pointer;
#if !FOUNDATION_LITTLE_ENDIAN
	byteswap8(&value);
#endif
	return value;
}

f32 Binary_Parser::read_f32() {
	f32 *pointer = (f32 *) this->read(sizeof(f32));
	f32 value = *pointer;
#if !FOUNDATION_LITTLE_ENDIAN
	byteswap4(&value);
#endif
	return value;
}

f64 Binary_Parser::read_f64() {
	f64 *pointer = (f64 *) this->read(sizeof(f64));
	f64 value = *pointer;
#if !FOUNDATION_LITTLE_ENDIAN
	byteswap8(&value);
#endif
	return value;
}

string Binary_Parser::read_string() {
	s64 count = this->read_s64();
	u8 *pointer = (u8 *) this->read(count);
	return string_view(pointer, count);
}



/* ---------------------------------------------- Binary_Writer ---------------------------------------------- */

void Binary_Writer::create(string file_path, s64 buffer_size) {
	this->buffer_position = 0;
	this->complete_size   = 0;
	this->buffer_size     = buffer_size;
	this->file_path       = copy_string(Default_Allocator, file_path);
	this->buffer          = (u8 *) Default_Allocator->allocate(this->buffer_size);
	this->append          = false;
}

void Binary_Writer::destroy() {
	this->flush();
	deallocate_string(Default_Allocator, &this->file_path);
	Default_Allocator->deallocate(this->buffer);
	this->complete_size = 0;
}

void Binary_Writer::flush() {
	s64 directory_end = os_search_path_for_directory_slash_reverse(this->file_path);
	if(directory_end != -1) os_create_directory(substring_view(this->file_path, 0, directory_end));
	os_write_file(this->file_path, string_view(this->buffer, this->buffer_position), this->append);
	this->append = true;
	this->buffer_position = 0;
}

void Binary_Writer::write(const void *data, s64 size) {
	this->complete_size += size;

	s64 data_offset = 0;
	while(size > 0) {
		s64 batch_size = min(this->buffer_size - this->buffer_position, size);
		memcpy(&this->buffer[this->buffer_position], &((u8 *) data)[data_offset], batch_size);
		this->buffer_position += batch_size;
		data_offset += batch_size;
		size -= batch_size;

		if(this->buffer_position == this->buffer_size) this->flush();
	}
}

void Binary_Writer::write_u8(u8 value) {
	this->write(&value, sizeof(u8));
}

void Binary_Writer::write_u16(u16 value) {
#if !FOUNDATION_LITTLE_ENDIAN
	byteswap2(&value);
#endif
	this->write(&value, sizeof(u16));
}

void Binary_Writer::write_u32(u32 value) {
#if !FOUNDATION_LITTLE_ENDIAN
	byteswap4(&value);
#endif
	this->write(&value, sizeof(u32));
}

void Binary_Writer::write_u64(u64 value) {
#if !FOUNDATION_LITTLE_ENDIAN
	byteswap8(&value);
#endif
	this->write(&value, sizeof(u64));
}

void Binary_Writer::write_s8(s8 value) {
	this->write(&value, sizeof(s8));
}

void Binary_Writer::write_s16(s16 value) {
#if !FOUNDATION_LITTLE_ENDIAN
	byteswap2(&value);
#endif
	this->write(&value, sizeof(s16));
}

void Binary_Writer::write_s32(s32 value) {
#if !FOUNDATION_LITTLE_ENDIAN
	byteswap4(&value);
#endif
	this->write(&value, sizeof(s32));
}

void Binary_Writer::write_s64(s64 value) {
#if !FOUNDATION_LITTLE_ENDIAN
	byteswap8(&value);
#endif
	this->write(&value, sizeof(s64));
}

void Binary_Writer::write_f32(f32 value) {
#if !FOUNDATION_LITTLE_ENDIAN
	byteswap4(&value);
#endif
	this->write(&value, sizeof(f32));
}

void Binary_Writer::write_f64(f64 value) {
#if !FOUNDATION_LITTLE_ENDIAN
	byteswap8(&value);
#endif
	this->write(&value, sizeof(f64));
}

void Binary_Writer::write_string(string value) {
	this->write_s64(value.count);
	this->write(value.data, value.count);
}

void Binary_Writer::write_null_terminated_string(string value) {
    this->write(value.data, value.count);
    this->write_u8(0);
}
