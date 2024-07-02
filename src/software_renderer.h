#pragma once

#include "foundation.h"
#include "string_type.h"

struct Window;

struct Color {
    u8 r, g, b, a;
    
    Color() = default;
    Color(u8 r, u8 g, u8 b, u8 a) : r(r), g(g), b(b), a(a) {};
};

struct Texture {
    s32 w, h;
    u8 channels;
    u8 *buffer;
};

struct Frame_Buffer {
    s32 w, h;
    u8 channels;
    u8 *buffer;
};

/* ------------------------------------------------- Texture ------------------------------------------------- */

void create_texture_from_file(Texture *texture, string file_path);
void destroy_texture(Texture *texture);



/* ----------------------------------------------- Frame Buffer ----------------------------------------------- */

void create_frame_buffer(Frame_Buffer *frame_buffer, s32 w, s32 h, u8 channels);
void destroy_frame_buffer(Frame_Buffer *frame_buffer);
void bind_frame_buffer(Frame_Buffer *frame_buffer);
void swap_buffers(Window *dst, Frame_Buffer *src);



/* ----------------------------------------------- Draw Commands ----------------------------------------------- */

void clear_frame(Color color);
void draw_quad(s32 x0, s32 y0, s32 x1, s32 y1, Color color0, Color color1, Color color2, Color color3);
void draw_quad(s32 x0, s32 y0, s32 x1, s32 y1, Color color);
void draw_quad(s32 x0, s32 y0, s32 x1, s32 y1, Texture *texture);