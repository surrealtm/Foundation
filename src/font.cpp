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
    u8 atlas_channels;
    s16 line_height;
    FT_Render_Mode render_options;
};


static
s16 freetype_unit_to_pixels(FT_Face face, FT_Pos unit) {
    return (s16) (FT_MulFix(unit, face->size->metrics.y_scale) >> 6);
}

static
Font_Glyph *find_default_glyph(Font *font) {
    assert(font->glyph_count);
    return &font->glyphs[0];
}

static
Font_Glyph *find_glyph(Font *font, s64 character) {
    if(character >= 0 && character <= 255) {
        return font->glyphs + font->extended_ascii_hot_path[character];
    }

    foundation_error("The character value '%u' is not supported in the font.", character);
    return null;
}

static
s64 find_glyph_index_in_advance_table(s64 character) {
    if(character == 0) return 0;
    
    if(character >= 32 && character <= 255) {
        return character - 31;
    }

    return -1;
}

static
s16 find_glyph_advance(Font_Glyph *glyph, s64 next_character) {
    s64 index = find_glyph_index_in_advance_table(next_character);
    if(index == -1) return glyph->advances[0];
    return glyph->advances[index];
}

static
Font_Atlas *find_atlas(Font *font, s64 index) {
    if(index == 0) return font->atlas; // Fast Path. Not tested if this is actually worth it.
    
    Font_Atlas *atlas = font->atlas;

    while(index > 0) {
        atlas = atlas->next;
        --index;
    }

    return atlas;
}


static
u8 add_glyph_to_font_atlas(Font *font, Font_Glyph *glyph, u8 *bitmap, s64 bitmap_pitch, s64 bitmap_channels, Font_Creation_Helper *helper) {
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
        atlas->channels = helper->atlas_channels;
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
            s64 source_offset = ((s64) y * bitmap_pitch + (s64) x * (s64) bitmap_channels);
            s64 destination_x = (s64) atlas->cursor_x + x;
            s64 destination_y = (s64) atlas->cursor_y + y;
            s64 destination_offset = (destination_x + destination_y * atlas->w) * atlas->channels;

            // Copy each channel into the destination
            for(u8 i = 0; i < atlas->channels; ++i) {
                atlas->bitmap[destination_offset + i] = bitmap[source_offset + i];
            }
        }
    }

    glyph->bitmap_offset_x = atlas->cursor_x;
    glyph->bitmap_offset_y = atlas->cursor_y;
    
    atlas->cursor_x += glyph->bitmap_width;
    
    return atlas_index;
}

static
u32 load_glyph(Font *font, FT_ULong character, Font_Creation_Helper *helper) {
    FT_UInt glyph_index = FT_Get_Char_Index(helper->face, character);

    // Char Index 0 is 'undefined', happens for ascii control characters. Only the first character (as a default
    // fallback) should be loaded (it will be a square icon). This character will be used as a placeholder for
    // other not-loaded glyphs (such as umlauts)
    if(glyph_index == 0 && character > 0) return 0;

    FT_Load_Glyph(helper->face, glyph_index, FT_LOAD_DEFAULT);
    FT_Render_Glyph(helper->face->glyph, helper->render_options);

    s16 bitmap_channels = helper->face->glyph->bitmap.pixel_mode == FT_PIXEL_MODE_LCD ? 3 : 1;

    Font_Glyph *glyph      = &font->glyphs[font->glyph_count];
    glyph->code            = character;
    glyph->cursor_offset_x = helper->face->glyph->bitmap_left;
    glyph->cursor_offset_y = helper->face->glyph->bitmap_top;
    glyph->bitmap_width    = helper->face->glyph->bitmap.width / bitmap_channels;
    glyph->bitmap_height   = helper->face->glyph->bitmap.rows;
    ++font->glyph_count;

    s16 unkerned_advance = (s16) (helper->face->glyph->advance.x >> 6);

    const s64 advance_count = 224; // We only support kerning between Ascii characters (for now).
    glyph->advances = (s16 *) Default_Allocator->allocate(advance_count * sizeof(s16));

    if(helper->apply_kerning) {
        for(s64 i = 0; i < font->glyphs_allocated; ++i) {
            s64 advance_index = find_glyph_index_in_advance_table(i);
            if(advance_index == -1) continue;
            
            assert(advance_index >= 0 && advance_index < advance_count);

            FT_UInt other_glyph_index = FT_Get_Char_Index(helper->face, (FT_ULong) i);
            FT_Vector kerning_value;
            FT_Get_Kerning(helper->face, glyph_index, other_glyph_index, FT_KERNING_DEFAULT, &kerning_value);
            glyph->advances[advance_index] = unkerned_advance + (s16) (kerning_value.x >> 6);
        }
    } else {
        for(s64 i = 0; i < font->glyphs_allocated; ++i) {
            s64 advance_index = find_glyph_index_in_advance_table(i);
            if(advance_index == -1) continue;

            assert(advance_index >= 0 && advance_index < advance_count);

            glyph->advances[advance_index] = unkerned_advance;
        }
    }

    if(glyph->bitmap_width > 0 && glyph->bitmap_height) {
        // Add the glyph to some font atlas.
        glyph->atlas_index = add_glyph_to_font_atlas(font, glyph, helper->face->glyph->bitmap.buffer, helper->face->glyph->bitmap.pitch, bitmap_channels, helper);
    } else {
        // The glyph does not have a bitmap attached.
        glyph->atlas_index = -1;
    }

    return (u32) (font->glyph_count - 1);
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



b8 create_font_from_file(Font *font, string file_path, s16 size, Font_Filter filter, Glyph_Set glyphs_to_load) {
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

    s32 xdpi, ydpi;
    os_get_desktop_dpi(&xdpi, &ydpi);
    FT_Set_Char_Size(face, 0, size * 72, xdpi, ydpi);

    font->line_height  = freetype_unit_to_pixels(face, face->height);
    font->ascender     = freetype_unit_to_pixels(face, face->ascender);
    font->descender    = freetype_unit_to_pixels(face, face->descender);
    font->glyph_height = font->ascender - font->descender;

    Font_Creation_Helper creation_helper;
    creation_helper.face          = face;
    creation_helper.line_height   = font->line_height;
    creation_helper.apply_kerning = FT_HAS_KERNING(face);

    if(filter != FONT_FILTER_Mono) {
        FT_Library_SetLcdFilter(library, FT_LCD_FILTER_DEFAULT);
        creation_helper.render_options  = FT_RENDER_MODE_LCD;
        creation_helper.atlas_channels = (filter == FONT_FILTER_Lcd_With_Alpha) ? 4 : 3;
    } else {
        creation_helper.render_options  = FT_RENDER_MODE_NORMAL;
        creation_helper.atlas_channels = 1;
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



Text_Mesh build_text_mesh(Font *font, string text, s32 x, s32 y, Text_Alignment alignment, Allocator *allocator) {
    assert(font->glyph_count > 0);

    Text_Mesh text_mesh;
    text_mesh.vertex_count  = text.count * 6;
    text_mesh.vertices      = (f32 *) allocator->allocate(text_mesh.vertex_count * 2 * sizeof(f32));
    text_mesh.uvs           = (f32 *) allocator->allocate(text_mesh.vertex_count * 2 * sizeof(f32));
    text_mesh.atlas_indices = (s32 *) allocator->allocate(text_mesh.vertex_count * sizeof(s32));

    s32 cx = x, cy = y;

    if(alignment & TEXT_ALIGNMENT_Centered) {
        s32 width = get_string_width_in_pixels(font, text);
        cx -= width / 2;
    } else if(alignment & TEXT_ALIGNMENT_Right) {
        s32 width = get_string_width_in_pixels(font, text);
        cx -= width;
    }

    if(alignment & TEXT_ALIGNMENT_Top) {
        cy += font->ascender;
    } else if(alignment & TEXT_ALIGNMENT_Median) {
        cy += (font->ascender + font->descender) / 2;
    }

    for(s64 i = 0; i < text.count; ++i) {
        u8 character = text[i];
        Font_Glyph *glyph = find_glyph(font, character);
        if(!glyph || glyph->atlas_index == -1) glyph = find_default_glyph(font);

        Font_Atlas *atlas = find_atlas(font, glyph->atlas_index);

        {
            s64 offset = i * 6 * 2;
            s32 x0 = (cx + glyph->cursor_offset_x), y0 = (cy - glyph->cursor_offset_y);
            s32 x1 = (cx + glyph->cursor_offset_x + glyph->bitmap_width), y1 = (cy - glyph->cursor_offset_y + glyph->bitmap_height);
            text_mesh.vertices[offset + 0]  = (f32) x0;
            text_mesh.vertices[offset + 1]  = (f32) y0;
            text_mesh.vertices[offset + 2]  = (f32) x1;
            text_mesh.vertices[offset + 3]  = (f32) y0;
            text_mesh.vertices[offset + 4]  = (f32) x0;
            text_mesh.vertices[offset + 5]  = (f32) y1;
            text_mesh.vertices[offset + 6]  = (f32) x0;
            text_mesh.vertices[offset + 7]  = (f32) y1;
            text_mesh.vertices[offset + 8]  = (f32) x1;
            text_mesh.vertices[offset + 9]  = (f32) y0;
            text_mesh.vertices[offset + 10] = (f32) x1;
            text_mesh.vertices[offset + 11] = (f32) y1;
        }

        {
            s64 offset = i * 6 * 2;
            f32 x0 = ((f32) glyph->bitmap_offset_x / (f32) atlas->w), y0 = ((f32) glyph->bitmap_offset_y / (f32) atlas->h);
            f32 x1 = ((f32) (glyph->bitmap_offset_x + glyph->bitmap_width) / (f32) atlas->w), y1 = ((f32) (glyph->bitmap_offset_y + glyph->bitmap_height) / (f32) atlas->h);
            text_mesh.uvs[offset + 0]  = x0;
            text_mesh.uvs[offset + 1]  = y0;
            text_mesh.uvs[offset + 2]  = x1;
            text_mesh.uvs[offset + 3]  = y0;
            text_mesh.uvs[offset + 4]  = x0;
            text_mesh.uvs[offset + 5]  = y1;
            text_mesh.uvs[offset + 6]  = x0;
            text_mesh.uvs[offset + 7]  = y1;
            text_mesh.uvs[offset + 8]  = x1;
            text_mesh.uvs[offset + 9]  = y0;
            text_mesh.uvs[offset + 10] = x1;
            text_mesh.uvs[offset + 11] = y1;
        }

        {
            s64 offset = i * 6;
            for(s64 i = 0; i < 6; ++i) {
                text_mesh.atlas_indices[offset + i] = glyph->atlas_index;
            }
        }

        if(i + 1 < text.count) cx += find_glyph_advance(glyph, text[i + 1]);
    }

    return text_mesh;
}

void free_text_mesh(Text_Mesh *text_mesh, Allocator *allocator) {
    allocator->deallocate(text_mesh->vertices);
    allocator->deallocate(text_mesh->uvs);
    allocator->deallocate(text_mesh->atlas_indices);

    text_mesh->vertex_count  = 0;
    text_mesh->vertices      = null;
    text_mesh->uvs           = null;
    text_mesh->atlas_indices = null;
}


s32 get_character_width_in_pixels(Font *font, u8 character) {
    Font_Glyph *glyph = find_glyph(font, character);
    if(!glyph) return 0;

    return glyph->bitmap_width;
}

s32 get_string_width_in_pixels(Font *font, string text) {
    s32 width = 0;

    for(s64 i = 0; i < text.count; ++i) {
        Font_Glyph *glyph = find_glyph(font, text[i]);
        if(!glyph || glyph->atlas_index == -1) glyph = find_default_glyph(font);
        
        if(i == 0) width -= glyph->cursor_offset_x;

        if(i + 1 < text.count) {
            width += find_glyph_advance(glyph, text[i + 1]);
        } else {
            width += glyph->bitmap_width + glyph->cursor_offset_x;
        }
    }

    return width;
}