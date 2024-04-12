#include "fileio.h"
#include "memutils.h"



/* ---------------------------------------------- Binary_Writer ---------------------------------------------- */

void Binary_Parser::create_from_string(string data) {
	this->data = data;
	this->position = 0;
}

void Binary_Parser::create_from_buffer(u8 *data, s64 size) {
	this->data = string_view(data, size);
	this->position = 0;
}

void Binary_Parser::create_from_file(string file_path) {
	this->data = os_read_file(Default_Allocator, file_path);
	this->position = 0;
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
#if !LITTLE_ENDIAN
	byteswap2(&value);
#endif
	return value;
}

u32 Binary_Parser::read_u32() {
	u32 *pointer = (u32 *) this->read(sizeof(u32));
	u32 value = *pointer;
#if !LITTLE_ENDIAN
	byteswap4(&value);
#endif
	return value;
}

u64 Binary_Parser::read_u64() {
	u64 *pointer = (u64 *) this->read(sizeof(u64));
	u64 value = *pointer;
#if !LITTLE_ENDIAN
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
#if !LITTLE_ENDIAN
	byteswap2(&value);
#endif
	return value;
}

s32 Binary_Parser::read_s32() {
	s32 *pointer = (s32 *) this->read(sizeof(s32));
	s32 value = *pointer;
#if !LITTLE_ENDIAN
	byteswap4(&value);
#endif
	return value;
}

s64 Binary_Parser::read_s64() {
	s64 *pointer = (s64 *) this->read(sizeof(s64));
	s64 value = *pointer;
#if !LITTLE_ENDIAN
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
	this->buffer_size     = buffer_size;
	this->file_path       = copy_string(Default_Allocator, file_path);
	this->buffer          = (u8 *) Default_Allocator->allocate(this->buffer_size);
	this->append          = false;
}

void Binary_Writer::destroy() {
	this->flush();
	deallocate_string(Default_Allocator, &this->file_path);
	Default_Allocator->deallocate(this->buffer);
}

void Binary_Writer::flush() {
	s64 directory_end = os_search_path_for_directory_slash_reverse(this->file_path);
	if(directory_end != -1) os_create_directory(substring_view(this->file_path, 0, directory_end));
	os_write_file(this->file_path, string_view(this->buffer, this->buffer_position), this->append);
	this->append = true;
	this->buffer_position = 0;
}

void Binary_Writer::write(void *data, s64 size) {
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
#if !LITTLE_ENDIAN
	byteswap2(&value);
#endif
	this->write(&value, sizeof(u16));
}

void Binary_Writer::write_u32(u32 value) {
#if !LITTLE_ENDIAN
	byteswap4(&value);
#endif
	this->write(&value, sizeof(u32));
}

void Binary_Writer::write_u64(u64 value) {
#if !LITTLE_ENDIAN
	byteswap8(&value);
#endif
	this->write(&value, sizeof(u64));
}

void Binary_Writer::write_s8(s8 value) {
	this->write(&value, sizeof(s8));
}

void Binary_Writer::write_s16(s16 value) {
#if !LITTLE_ENDIAN
	byteswap2(&value);
#endif
	this->write(&value, sizeof(s16));
}

void Binary_Writer::write_s32(s32 value) {
#if !LITTLE_ENDIAN
	byteswap4(&value);
#endif
	this->write(&value, sizeof(s32));
}

void Binary_Writer::write_s64(s64 value) {
#if !LITTLE_ENDIAN
	byteswap8(&value);
#endif
	this->write(&value, sizeof(s64));
}

void Binary_Writer::write_string(string value) {
	this->write_s64(value.count);
	this->write(value.data, value.count);
}
