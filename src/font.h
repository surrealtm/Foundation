#pragma once

#include "foundation.h"
#include "strings.h"

enum Text_Alignment {
    TEXT_ALIGNMENT_Left     = 0x1,
    TEXT_ALIGNMENT_Centered = 0x2,
    TEXT_ALIGNMENT_Right    = 0x4,
};

enum Glyph_Set {
    GLYPH_SET_None  = 0x0,
    GLYPH_SET_Ascii = 0x1,
    GLYPH_SET_Extended_Ascii = 0x2,
};

BITWISE(Glyph_Set);

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
     // The offset in pixels from the current cursor position to where the bitmap of this glyph should be placed.
    s8 cursor_offset_x;
    s8 cursor_offset_y;

    // Which font atlas this glyph is stored in.
    u8 atlas_index;

    // The bitmap size of this glyph inside the font atlas. This glyph's bitmap should be blitted onto the
    // screen in a 1:1 ratio for optimal quality.
    u16 bitmap_width;
    u16 bitmap_height;
    u16 bitmap_offset_x;
    u16 bitmap_offset_y;

    // The horizontal advances (including kerning) in pixels to every other glyph.
    s16 *advances;
};

struct Font {
    // Font bitmap. The font may require multiple textures, depending on the glyph
    // count and the selected size. These are stored in a linked list.
    Font_Atlas *atlas;

    // Font metrics, all in pixels.
    s16 ascender; // Distance between the top of the 'default' character box to the baseline.
    s16 descender; // Distance between the bottom of the 'default' character box to the baseline.
    s16 line_height; // Recommended distance between baselines.
    s16 glyph_height; // Ascender - Descender
    
    // Internal layout information.
    Glyph_Set loaded_glyph_sets;
    Font_Glyph *extended_ascii_hot_path[256];

    Font_Glyph *glyphs;
    s64 glyph_count;
    s64 glyphs_allocated;
};

b8 create_font_from_file(Font *font, string file_path, s16 size, b8 use_lcd_filtering, Glyph_Set glyphs_to_load);
void destroy_font(Font *font);
