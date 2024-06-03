#pragma once

#include "foundation.h"
#include "string_type.h"
#include "error.h"

enum Text_Alignment {
    /* Horizontal Alignment */
    TEXT_ALIGNMENT_Left     = 0x1,
    TEXT_ALIGNMENT_Centered = 0x2,
    TEXT_ALIGNMENT_Right    = 0x4,

    /* Vertical Alignment */
    TEXT_ALIGNMENT_Top    = 0x8,
    TEXT_ALIGNMENT_Median = 0x10,
    TEXT_ALIGNMENT_Bottom = 0x20,
};

BITWISE(Text_Alignment);

enum Glyph_Set {
    GLYPH_SET_None  = 0x0,
    GLYPH_SET_Ascii = 0x1,
    GLYPH_SET_Extended_Ascii = 0x2,
};

BITWISE(Glyph_Set);

enum Font_Filter {
    FONT_FILTER_Mono, // One channel.
    FONT_FILTER_Lcd_Without_Alpha, // Three channels.
    FONT_FILTER_Lcd_With_Alpha, // This will enforce four channels in the atlas bitmap (which is required for certain Graphics APIs). The alpha value will always be 255.
};

struct Font_Atlas {
    Font_Atlas *next;
    u8 *bitmap;
    s16 w;
    s16 h;
    u8 channels;
    void *user_handle; // This can (but doesn't have to be) filled out by user-level drawing code for an easier time rendering. It can represent a texture object containing this atlas.

    s16 cursor_x;
    s16 cursor_y;
};

struct Font_Glyph {
    u32 code;

     // The offset in pixels from the current cursor position to where the bitmap of this glyph should be placed.
    s16 cursor_offset_x;
    s16 cursor_offset_y;

    // The bitmap size of this glyph inside the font atlas. This glyph's bitmap should be blitted onto the
    // screen in a 1:1 ratio for optimal quality.
    u16 bitmap_width;
    u16 bitmap_height;
    u16 bitmap_offset_x;
    u16 bitmap_offset_y;

    // Which font atlas this glyph is stored in.
    u16 atlas_index;

    // The horizontal advances (including kerning) in pixels to every other glyph.
    s16 *advances;
};

struct Font {
    // Font bitmap. The font may require multiple textures, depending on the glyph
    // count and the selected size. These are stored in a linked list.
    Font_Filter filter; // Whatever the user passed in create_font_from_file.
    Font_Atlas *atlas;

    // Font metrics, all in pixels.
    s16 ascender; // Distance between the top of the 'default' character box to the baseline.
    s16 descender; // Distance between the bottom of the 'default' character box to the baseline.
    s16 line_height; // Recommended distance between baselines.
    s16 glyph_height; // Ascender - Descender
    
    // Internal layout information.
    Glyph_Set loaded_glyph_sets;
    u32 extended_ascii_hot_path[256]; // Indices into the glyph array as a shortcut so that we don't have to actually search for it. We use indices here since the glyphs array may grow.

    Font_Glyph *glyphs;
    s64 glyph_count;
    s64 glyphs_allocated;
};

struct Text_Mesh {
    s64 glyph_count;
    f32 *vertices; // 6 Vertices per glyph. 2 Floats per Vertex.
    f32 *uvs; // 6 Uvs per glyph. 2 Floats per Vertex.
    Font_Atlas **atlasses; // 1 Atlas per glyph.
};

Error_Code create_font_from_file(Font *font, string file_path, s16 size, Font_Filter filter, Glyph_Set glyphs_to_load);
Error_Code create_font_from_memory(Font *font, string _data, s16 size, Font_Filter filter, Glyph_Set glyphs_to_load);
void destroy_font(Font *font);

Text_Mesh build_text_mesh(Font *font, string text, s32 x, s32 y, Text_Alignment alignment, Allocator *allocator);
void free_text_mesh(Text_Mesh *text_mesh, Allocator *allocator);

s32 get_character_width_in_pixels(Font *font, u8 character);
s32 get_string_width_in_pixels(Font *font, string text);
