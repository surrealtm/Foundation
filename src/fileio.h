#pragma once

#include "foundation.h"
#include "strings.h"


/* ----------------------------------------------- Ascii Files ----------------------------------------------- */

struct Ascii_Parser {
    string data;
    s64 position;

    void create_from_string(string data);
    void create_from_buffer(u8 *data, s64 size);
    void create_from_file(string file_path);
    void destroy_file_data();

    string read_string();
    u8 read_u8();
    u16 read_u16();
    u32 read_u32();
    u64 read_u64();
    s8 read_s8();
    s16 read_s16();
    s32 read_s32();
    s64 read_s64();
    f32 read_f32();
    f64 read_f64();
};

struct Ascii_Writer {
    string file_path;
	String_Builder builder;

    void create(string file_path, s64 buffer_size);
    void destroy();
    void flush();

    void write_string(string data);
    void write_u8(u8 value);
    void write_u16(u16 value);
    void write_u32(u32 value);
    void write_u64(u64 value);
    void write_s8(s8 value);
    void write_s16(s16 value);
    void write_s32(s32 value);
    void write_s64(s64 value);
    void write_f32(f32 value);
    void write_f64(f64 value);
};



/* ----------------------------------------------- Binary Files ----------------------------------------------- */

struct Binary_Parser {
	string data;
	s64 position;

	void create_from_string(string data);
	void create_from_buffer(u8 *data, s64 size);
	void create_from_file(string file_path);
	void destroy_file_data();

	void *read(s64 size_in_bytes);
	u8 read_u8();
	u16 read_u16();
	u32 read_u32();
	u64 read_u64();
	s8 read_s8();
	s16 read_s16();
	s32 read_s32();
	s64 read_s64();
	f32 read_f32();
	f64 read_f64();
	string read_string();
};

struct Binary_Writer {
	string file_path;
	u8 *buffer;
	s64 buffer_size;
	s64 buffer_position;
	b8 append;

	void create(string file_path, s64 buffer_size);
	void destroy();
	void flush();

	void write(void *data, s64 size_in_bytes);
	void write_u8(u8 value);
	void write_u16(u16 value);
	void write_u32(u32 value);
	void write_u64(u64 value);
	void write_s8(s8 value);
	void write_s16(s16 value);
	void write_s32(s32 value);
	void write_s64(s64 value);
	void write_f32(f32 value);
	void write_f64(f64 value);
	void write_string(string value);
};
