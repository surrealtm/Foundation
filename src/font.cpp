#include "font.h"
#include "memutils.h"
#include "os_specific.h"

#define FT_CONFIG_OPTION_SUBPIXEL_RENDERING

#include "Dependencies/freetype/freetype.h"
#include "Dependencies/freetype/ftlcdfil.h"

#define FONT_ATLAS_SIZE 512

struct Font_Creation_Helper {
    FT_Face face;
    b8 apply_kerning;
    u8 bitmap_channels;
    s16 line_height;
    FT_Render_Mode render_options;
};

static
Font_Glyph *find_glyph(Font *font, FT_ULong character) {
    if(character >= 0 && character <= 255) {
        return font->extended_ascii_hot_path[character];
    }

    foundation_error("The character value '%ul' is not supported in the font.", character);
    return null;
}

static
u8 add_glyph_to_font_atlas(Font *font, Font_Glyph *glyph, u8 *bitmap, s64 bitmap_pitch, Font_Creation_Helper *helper) {
    assert(glyph->bitmap_height <= helper->line_height); // We assume that the line height covers all glyphs, so that we don't have any overlaps in the font atlas.

    //
    // Find an existing atlas which still has enough space for this new glyph.
    //
    Font_Atlas *atlas = font->atlas;
    u8 atlas_index = 0;

    while(atlas) {
        if((atlas->cursor_y + glyph->bitmap_height < atlas->h && atlas->cursor_x + glyph->bitmap_width < atlas->w) || (atlas->cursor_y + helper->line_height + glyph->bitmap_height < atlas->h && glyph->bitmap_width < atlas->w)) {
            // If the glyph fits into this atlas, then we have found our target and should stop. There are two
            // ways this could fit:
            //   1. Inside the current line by just appending the bitmap to the right of the cursor.
            //   2. By moving the cursor to the next line and starting from the left.
            break;
        }

        atlas = atlas->next;
        ++atlas_index;
    }

    if(!atlas) {
        //
        // Create a new font atlas.
        //
        atlas = (Font_Atlas *) Default_Allocator->New<Font_Atlas>();
        atlas->w        = FONT_ATLAS_SIZE;
        atlas->h        = FONT_ATLAS_SIZE;
        atlas->channels = helper->bitmap_channels;
        atlas->bitmap   = (u8 *) Default_Allocator->allocate(atlas->w * atlas->h * atlas->channels);

        if(font->atlas) {
            atlas_index = 0;

            Font_Atlas *tail = font->atlas;
            while(tail->next) {
                ++atlas_index;
                tail = tail->next;
            }
        
            ++atlas_index;
            tail->next = atlas;
        } else {
            font->atlas = atlas;
            atlas_index = 0;
        }
    }

    if(atlas->cursor_x + glyph->bitmap_width > atlas->w) {
        // Go into the next line inside the atlas.
        atlas->cursor_x = 0;
        atlas->cursor_y += helper->line_height;
        assert(atlas->cursor_y + glyph->bitmap_height <= atlas->h);
    }

    assert(atlas->cursor_x + glyph->bitmap_width <= atlas->w);

    // Copy the bitmap data from the internal freetype buffer into the glyph texture
    for(s16 y = 0; y < glyph->bitmap_height; ++y) {
        for(s16 x = 0; x < glyph->bitmap_width; ++x) {
            // When using LCD filtering, the bitmap has padding at the end of rows, so the 'pitch' value is
            // the width + padding, which we require when copying into our big atlas.
            s64 source_offset = ((s64) y * bitmap_pitch + (s64) x * (s64) atlas->channels);
            s64 destination_x = (s64) atlas->cursor_x + x;
            s64 destination_y = (s64) atlas->cursor_y + y;
            s64 destination_offset = (destination_x + destination_y * atlas->w) * atlas->channels;

            // Copy each channel into the destination
            for(u8 i = 0; i < atlas->channels; ++i) {
                atlas->bitmap[destination_offset + i] = bitmap[source_offset + i];
            }
        }
    }
    
    atlas->cursor_x += glyph->bitmap_width;
    
    return atlas_index;
}

static
Font_Glyph *load_glyph(Font *font, FT_ULong character, Font_Creation_Helper *helper) {
    FT_UInt glyph_index = FT_Get_Char_Index(helper->face, character);

    // Char Index 0 is 'undefined', happens for ascii control characters. Only the first character (as a default
    // fallback) should be loaded (it will be a square icon). This character will be used as a placeholder for
    // other not-loaded glyphs (such as umlauts)
    if(glyph_index == 0 && character > 0) return null;

    FT_Load_Glyph(helper->face, glyph_index, FT_LOAD_DEFAULT);
    FT_Render_Glyph(helper->face->glyph, helper->render_options);

    Font_Glyph *glyph      = &font->glyphs[font->glyph_count];
    glyph->cursor_offset_x = helper->face->glyph->bitmap_left;
    glyph->cursor_offset_y = helper->face->glyph->bitmap_top;
    glyph->bitmap_width    = helper->face->glyph->bitmap.width / helper->bitmap_channels;
    glyph->bitmap_height   = helper->face->glyph->bitmap.rows;
    ++font->glyph_count;

    s16 unkerned_advance = (s16) (helper->face->glyph->advance.x >> 6);

    glyph->advances = (s16 *) Default_Allocator->allocate(font->glyphs_allocated * sizeof(s16));

    if(helper->apply_kerning) {
        for(s64 i = 0; i < font->glyphs_allocated; ++i) {
            FT_UInt other_glyph_index = FT_Get_Char_Index(helper->face, (FT_ULong) i);
            FT_Vector kerning_value;
            FT_Get_Kerning(helper->face, glyph_index, other_glyph_index, FT_KERNING_DEFAULT, &kerning_value);
            glyph->advances[i] = unkerned_advance + (s16) (kerning_value.x >> 6);
        }
    } else {
        for(s64 i = 0; i < font->glyphs_allocated; ++i) {
            glyph->advances[i] = unkerned_advance;
        }
    }

    if(glyph->bitmap_width > 0 && glyph->bitmap_height) {
        // Add the glyph to some font atlas.
        glyph->atlas_index = add_glyph_to_font_atlas(font, glyph, helper->face->glyph->bitmap.buffer, helper->face->glyph->bitmap.pitch, helper);
    } else {
        // The glyph does not have a bitmap attached.
        glyph->atlas_index = -1;
    }

    return glyph;
}

static
void allocate_additional_glyphs(Font *font, s64 count) {
    if(font->glyphs_allocated) {
        font->glyphs = (Font_Glyph *) Default_Allocator->reallocate(font->glyphs, (font->glyphs_allocated + count) * sizeof(Font_Glyph));
        font->glyphs_allocated += count;
    } else {
        font->glyphs = (Font_Glyph *) Default_Allocator->allocate(count * sizeof(Font_Glyph));
        font->glyphs_allocated = count;
    }
}

static
void load_glyph_set(Font *font, Glyph_Set glyphs_to_load, Font_Creation_Helper *helper) {
    if(font->loaded_glyph_sets & glyphs_to_load) return;

    switch(glyphs_to_load) {
    case GLYPH_SET_Ascii: {
        allocate_additional_glyphs(font, 128);
        
        font->extended_ascii_hot_path[0] = load_glyph(font, 0, helper);
        
        for(FT_ULong i = 32; i <= 127; ++i) {
            font->extended_ascii_hot_path[i] = load_glyph(font, i, helper);
        }

        font->loaded_glyph_sets |= GLYPH_SET_Ascii;
    } break;

    case GLYPH_SET_Extended_Ascii: {
        load_glyph_set(font, GLYPH_SET_Ascii, helper);
        allocate_additional_glyphs(font, 127);
        
        for(FT_ULong i = 128; i <= 255; ++i) {
            font->extended_ascii_hot_path[i] = load_glyph(font, i, helper);        
        }
        
        font->loaded_glyph_sets |= GLYPH_SET_Extended_Ascii;
    } break;
    }
}

b8 create_font_from_file(Font *font, string file_path, s16 size, b8 use_lcd_filtering, Glyph_Set glyphs_to_load) {
    memset(font, 0, sizeof(Font)); // Make sure we don't have any uninitialized data in here.

    string file_content = os_read_file(Default_Allocator, file_path);
    if(!file_content.count) {
        foundation_error("Failed to load the font '%.*s' from disk: The file does not exist.", (u32) file_path.count, file_path.data);
        return false;
    }

    defer { os_free_file_content(Default_Allocator, &file_content); };

    FT_Library library;
    if(FT_Init_FreeType(&library)) {
        foundation_error("Failed to initialize the FreeType library.");
        return false;
    }

    defer { FT_Done_FreeType(library); };    

    FT_Face face;
    if(FT_New_Memory_Face(library, file_content.data, (FT_Long) file_content.count, 0, &face)) {
        foundation_error("Failed to create the FreeType font face.");
        return false;
    }

    defer { FT_Done_Face(face); };

    FT_Set_Pixel_Sizes(face, 0, size);

    font->line_height  = (s16) (face->size->metrics.height >> 6);
    font->ascender     = (s16) (face->size->metrics.ascender >> 6);
    font->descender    = (s16) (face->size->metrics.descender >> 6);
    font->glyph_height = font->ascender - font->descender;

    Font_Creation_Helper creation_helper;
    creation_helper.face          = face;
    creation_helper.line_height   = font->line_height;
    creation_helper.apply_kerning = FT_HAS_KERNING(face);

    if(use_lcd_filtering) {
        FT_Library_SetLcdFilter(library, FT_LCD_FILTER_DEFAULT);
        creation_helper.render_options  = FT_RENDER_MODE_LCD;
        creation_helper.bitmap_channels = 3;
    } else {
        creation_helper.render_options  = FT_RENDER_MODE_NORMAL;
        creation_helper.bitmap_channels = 1;
    }

    load_glyph_set(font, glyphs_to_load, &creation_helper);
    
    return true;
}

void destroy_font(Font *font) {
    for(Font_Atlas *atlas = font->atlas; atlas != null; ) {
        Default_Allocator->deallocate(atlas->bitmap);
        Font_Atlas *next = atlas->next;
        Default_Allocator->deallocate(atlas);
        atlas = next;
    }

    font->atlas = null;

    font->ascender    = 0;
    font->descender   = 0;
    font->line_height = 0;
    
    for(s64 i = 0; i < font->glyph_count; ++i) {
        Default_Allocator->deallocate(font->glyphs[i].advances);
    }

    Default_Allocator->deallocate(font->glyphs);
    font->loaded_glyph_sets = GLYPH_SET_None;
    font->glyphs            = null;
    font->glyph_count       = 0;
    font->glyphs_allocated  = 0;
}
