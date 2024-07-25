#pragma once

#include "foundation.h"
#include "string_type.h"
#include "error.h"

enum Glyph_Set;
enum Text_Alignment;

struct Window;
struct Font;

enum Color_Format {
    COLOR_FORMAT_Unknown,
    COLOR_FORMAT_A,
    COLOR_FORMAT_R,
    COLOR_FORMAT_RG,
    COLOR_FORMAT_RGB,
    COLOR_FORMAT_RGBA,
    COLOR_FORMAT_BGRA,
    COLOR_FORMAT_COUNT,
};

struct Color {
    u8 r, g, b, a;
    
    Color() = default;
    Color(u8 r, u8 g, u8 b, u8 a) : r(r), g(g), b(b), a(a) {};
};

struct Texture {
    Color_Format format;
    s32 w, h;
    u8 *buffer;
};

struct Frame_Buffer {
    Color_Format format;
    s32 w, h;
    u8 *buffer;
};

struct Software_Font { // This is just a wrapper around a Font indicating that the font has been set up for software rendering, meaning the font's atlases have user_pointers pointing at Software Textures
    Font *underlying;
};



/* -------------------------------------------- Software Renderer -------------------------------------------- */

void create_software_renderer(Window *window);
void destroy_software_renderer();
void maybe_resize_back_buffer();



/* ------------------------------------------------- Texture ------------------------------------------------- */

Error_Code create_texture_from_file(Texture *texture, string file_path);
void create_texture_from_memory(Texture *texture, u8 *buffer, s32 w, s32 h, u8 channels); // This makes a copy from the buffer!
void create_texture_from_memory(Texture *texture, u8 *buffer, s32 w, s32 h, Color_Format format); // This makes a copy from the buffer!
void destroy_texture(Texture *texture);



/* ----------------------------------------------- Frame Buffer ----------------------------------------------- */

void create_frame_buffer(Frame_Buffer *frame_buffer, s32 w, s32 h, u8 channels);
void create_frame_buffer(Frame_Buffer *frame_buffer, s32 w, s32 h, Color_Format format);
void destroy_frame_buffer(Frame_Buffer *frame_buffer);
void bind_frame_buffer(Frame_Buffer *frame_buffer);
void unbind_frame_buffer();
void blit_frame_buffer(Frame_Buffer *dst, Frame_Buffer *src);
void swap_buffers(Frame_Buffer *src);
void swap_buffers();



/* ---------------------------------------------- Draw Commands ---------------------------------------------- */

void clear_frame(Color color);
void draw_quad(s32 x0, s32 y0, s32 x1, s32 y1, Color color0, Color color1, Color color2, Color color3);
void draw_quad(s32 x0, s32 y0, s32 x1, s32 y1, Color color);
void draw_quad(s32 x0, s32 y0, s32 x1, s32 y1, Texture *texture);
void draw_outlined_quad(s32 x0, s32 y0, s32 x1, s32 y1, s32 thickness, Color color);



/* ----------------------------------------------- Font Helpers ----------------------------------------------- */

Error_Code create_software_font_from_file(Software_Font *software_font, string file_path, s16 size, Glyph_Set glyphs_to_load);
void destroy_software_font(Software_Font *software_font);
void draw_text(Software_Font *software_font, string text, s32 x, s32 y, Text_Alignment alignment, Color color);
