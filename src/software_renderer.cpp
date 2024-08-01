#include "software_renderer.h"
#include "memutils.h"
#include "math/v2.h"
#include "math/v4.h"
#include "window.h"
#include "font.h"

#include "Dependencies/stb_image.h"

enum Draw_Command_Kind {
    DRAW_COMMAND_Nothing,
    DRAW_COMMAND_Clear,
    DRAW_COMMAND_Draw,
};

enum Draw_Command_Options {
    DRAW_OPTION_Nothing  = 0x0,
    DRAW_OPTION_Colored  = 0x1,
    DRAW_OPTION_Textured = 0x2,
    DRAW_OPTION_Blending = 0x4,
};

BITWISE(Draw_Command_Options);

struct Draw_AABB {
    v2f min, max;
};

struct Draw_Vertex {
    v2f position;
    v2f uv;
    Color color;
};

struct Draw_Command {
    Draw_Command_Kind kind;
    Draw_AABB scissors;    
    
    Frame_Buffer *frame_buffer;
    
    union {
        struct {
            Color color;
        } clear;
        
        struct {
            Texture *texture;
            Draw_Vertex *vertices;
            s32 vertex_count;
            
            Draw_Command_Options options;
        } draw;
    };
};

struct Software_Renderer {
    Window *window;

    Frame_Buffer back_buffer;    
    Frame_Buffer *bound_frame_buffer = null;

    Draw_AABB bound_scissors;
    
    b8 command_system_setup = false;
    Resizable_Array<Draw_Command> commands;
};

static thread_local Software_Renderer state;

/* -------------------------------------------------- Helpers -------------------------------------------------- */

enum Channel {
    CHANNEL_R = 0,
    CHANNEL_G = 1,
    CHANNEL_B = 2,
    CHANNEL_A = 3,
    CHANNEL_COUNT = 4,
};

static
u8 channels_per_pixel[COLOR_FORMAT_COUNT] = {
    0, // Unknown
    1, // A
    1, // R
    2, // RG
    3, // RGB
    4, // RGBA
    4, // BGRA
};

static
u8 channel_indices[COLOR_FORMAT_COUNT][CHANNEL_COUNT] = {
    { 0, 0, 0, 0 }, // Unknown
    { 0, 0, 0, 0 }, // A
    { 0, 0, 0, 0 }, // R
    { 0, 1, 0, 0 }, // RB
    { 0, 1, 2, 0 }, // RBG
    { 0, 1, 2, 3 }, // RBGA
    { 2, 1, 0, 3 }, // BGRA
};

static
Color_Format get_color_format_for_channels(u8 channels) {
    Color_Format format;
    
    switch(channels) {
    case 1:  format = COLOR_FORMAT_R;       break;
    case 2:  format = COLOR_FORMAT_RG;      break;
    case 3:  format = COLOR_FORMAT_RGB;     break;
    case 4:  format = COLOR_FORMAT_RGBA;    break;
    default: format = COLOR_FORMAT_Unknown; break;
    }

    return format;
}

static
u8 *get_pixel_in_frame_buffer(Frame_Buffer *frame_buffer, s32 x, s32 y) {
    s64 offset = ((s64) y * (s64) frame_buffer->w + x) * channels_per_pixel[frame_buffer->format];
    return &frame_buffer->buffer[offset];
}

static
u8 *get_pixel_in_texture(Texture *texture, s32 x, s32 y) {
    s64 offset = ((s64) y * (s64) texture->w + x) * channels_per_pixel[texture->format];
    return &texture->buffer[offset];
}

static
Color query_pixel(u8 *pixel, Color_Format format) {
    Color result;

    u8 channels = channels_per_pixel[format];

    if(format == COLOR_FORMAT_A) {
        result.r = result.g = result.b = 255;
        result.a = *pixel;
    } else {
        result.r = channels > 0 ? *(pixel + channel_indices[format][CHANNEL_R]) : 255;
        result.g = channels > 1 ? *(pixel + channel_indices[format][CHANNEL_G]) : 255;
        result.b = channels > 2 ? *(pixel + channel_indices[format][CHANNEL_B]) : 255;
        result.a = channels > 3 ? *(pixel + channel_indices[format][CHANNEL_A]) : 255;
    }

    return result;
}

static
Color query_texture(Texture *texture, v2f uv) {
    s32 x = (s32) roundf(uv.x * texture->w), y = (s32) roundf(uv.y * texture->h);
    
    if(x < 0 || x >= texture->w || y < 0 || y >= texture->h) return Color(255, 255, 255, 255);

    u8 *pixel = get_pixel_in_texture(texture, x, y);

    return query_pixel(pixel, texture->format);
}

static
void include_vertex_in_aabb(Draw_AABB *aabb, Draw_Vertex *vertex) {
    aabb->min.x = min(aabb->min.x, vertex->position.x);
    aabb->min.y = min(aabb->min.y, vertex->position.y);
    aabb->max.x = max(aabb->max.x, vertex->position.x);
    aabb->max.y = max(aabb->max.y, vertex->position.y);
}

static
Draw_AABB calculate_aabb_for_vertices(Draw_Vertex *v0, Draw_Vertex *v1, Draw_Vertex *v2) {
    Draw_AABB aabb;

    aabb.min = v2f(MAX_F32, MAX_F32);
    aabb.max = v2f(MIN_F32, MIN_F32);

    include_vertex_in_aabb(&aabb, v0);
    include_vertex_in_aabb(&aabb, v1);
    include_vertex_in_aabb(&aabb, v2);
    
    return aabb;
}

static
Draw_AABB intersect_aabb(Draw_AABB lhs, s32 width, s32 height) {
    return { v2f(max(lhs.min.x, (f32) 0),     max(lhs.min.y, (f32) 0)),
             v2f(min(lhs.max.x, (f32) width - 1), min(lhs.max.y, (f32) height - 1)) };
}

static
Draw_AABB intersect_aabb(Draw_AABB lhs, Draw_AABB rhs) {
    return { v2f(max(lhs.min.x, rhs.min.x), max(lhs.min.y, rhs.min.y)),
             v2f(min(lhs.max.x, rhs.max.x), min(lhs.max.y, rhs.max.y)) };    
}

static
b8 is_top_or_left_edge(const v2f &from, const v2f &to, const v2f &other) {
    return (from.y == to.y && from.y < other.y) // Top edge
        || (from.x <= other.x && to.x <= other.x); // Left edge
}

static
b8 point_inside_triangle(const v2f &p, const v2f &p0, const v2f &p1, const v2f &p2, f32 *s, f32 *t, f32 *u) {
    f32 A = (-p1.y * p2.x + p0.y * (-p1.x + p2.x) + p0.x * (p1.y - p2.y) + p1.x * p2.y) / 2.f;
    f32 sign = (A < 0) ? -1.f : 1.f;
    f32 _u = (p0.x * p1.y - p0.y * p1.x + (p0.y - p1.y) * p.x + (p1.x - p0.x) * p.y) * sign; // Edge p0 -> p1
    f32 _t = (p0.y * p2.x - p0.x * p2.y + (p2.y - p0.y) * p.x + (p0.x - p2.x) * p.y) * sign; // Edge p0 -> p2
    
    //
    // :FillRule
    // Two triangles that are not overlapping but share an edge (such as when rendering a quad)
    // need to make sure that the pixel on the shared edge gets processed once and only once.
    // To ensure that, we essentially ensure this point is only considered in one of the triangles,
    // by adding a bias on the other triangle (so that it is considered barely outside).
    // We put up the rule that the left-most edge of the triangle gets this bias of one pixel,
    // because two non-overlapping triangles cannot share their left-most edges, which means that
    // we guarantee only one triangle gets this bias, meaning that only one triangle will process
    // this pixel.
    // Note that if two corners have the same X coordinate, then the triangle is only one pixel wide,
    // in which case having a bias would lead to this triangle not being filled at all.
    //
    f32 ubias = is_top_or_left_edge(p0, p1, p2) ? 0.f : 1.f, 
        tbias = is_top_or_left_edge(p2, p0, p1) ? 0.f : 1.f, 
        sbias = is_top_or_left_edge(p1, p2, p0) ? 0.f : 1.f;

    f32 denom = 2.f * A * sign;
    
    b8 inside_triangle = _u >= ubias && _t >= tbias && (_u + _t) + sbias <= denom;
    if(!inside_triangle) return false;
    
    *t = _t / denom;
    *u = _u / denom;
    *s = 1.f - *t - *u;
    
    return true;
}

static
f32 interpolate(f32 s, f32 s_value, f32 t, f32 t_value, f32 u, f32 u_value) {
    return s * s_value + t * t_value + u * u_value;
}

static
Color interpolate(f32 s, Color s_color, f32 t, Color t_color, f32 u, Color u_color) {
    Color result;
    result.r = (u8) (interpolate(s, s_color.r / 255.f, t, t_color.r / 255.f, u, u_color.r / 255.f) * 255.f);
    result.g = (u8) (interpolate(s, s_color.g / 255.f, t, t_color.g / 255.f, u, u_color.g / 255.f) * 255.f);
    result.b = (u8) (interpolate(s, s_color.b / 255.f, t, t_color.b / 255.f, u, u_color.b / 255.f) * 255.f);
    result.a = (u8) (interpolate(s, s_color.a / 255.f, t, t_color.a / 255.f, u, u_color.a / 255.f) * 255.f);
    return result;
}

static
v2f interpolate(f32 s, v2f s_vector, f32 t, v2f t_vector, f32 u, v2f u_vector) {
    v2f result;
    result.x = interpolate(s, s_vector.x, t, t_vector.x, u, u_vector.x);
    result.y = interpolate(s, s_vector.y, t, t_vector.y, u, u_vector.y);
    return result;
}

static
u8 mix(u8 lhs, u8 rhs, f32 lhs_factor, f32 rhs_factor) {
    return (u8) (((lhs / 255.f) * lhs_factor + (rhs / 255.f) * rhs_factor) * 255.f);
}

static
Color mix(Color lhs, Color rhs, f32 lhs_factor, f32 rhs_factor) {
    Color result;
    result.r = mix(lhs.r, rhs.r, lhs_factor, rhs_factor);
    result.g = mix(lhs.g, rhs.g, lhs_factor, rhs_factor);
    result.b = mix(lhs.b, rhs.b, lhs_factor, rhs_factor);
    result.a = mix(lhs.a, rhs.a, lhs_factor, rhs_factor);
    return result;
}

static
Color mix(Color src, Color dst) { // src is the color of the currently rendererd command, dst is the color in the frame buffer
    f32 src_alpha = src.a / 255.f;
    f32 one_minus_src_alpha = 1.f - src_alpha;
    return mix(src, dst, src_alpha, one_minus_src_alpha);
}

static
Color multiply(Color lhs, Color rhs) {
    Color result;
    result.r = (u8) (((f32) lhs.r / 255.f) * ((f32) rhs.r / 255.f) * 255.f);
    result.g = (u8) (((f32) lhs.g / 255.f) * ((f32) rhs.g / 255.f) * 255.f);
    result.b = (u8) (((f32) lhs.b / 255.f) * ((f32) rhs.b / 255.f) * 255.f);
    result.a = (u8) (((f32) lhs.a / 255.f) * ((f32) rhs.a / 255.f) * 255.f);
    return result;
}



/* ------------------------------------------------ Draw Command ------------------------------------------------ */

static
Draw_Command *make_draw_command(Draw_Command_Kind kind) {
    if(!state.command_system_setup) {
        state.commands.allocator = Default_Allocator;
        state.command_system_setup = true;
    }
    
    Draw_Command *command = state.commands.push();
    command->kind         = kind;
    command->frame_buffer = state.bound_frame_buffer;
    command->scissors     = state.bound_scissors;
    return command;
}

static
Draw_Command *make_triangle_draw_command(s32 vertex_count) {
    Draw_Command *command      = make_draw_command(DRAW_COMMAND_Draw);
    command->draw.vertex_count = vertex_count;
    command->draw.vertices     = (Draw_Vertex *) Default_Allocator->allocate(command->draw.vertex_count * sizeof(Draw_Vertex));
    return command;
}

static
void destroy_draw_command(Draw_Command *command) {
    switch(command->kind) {
    case DRAW_COMMAND_Draw:
        Default_Allocator->deallocate(command->draw.vertices);    
        break;
    }
}

static
void draw_triangle(Draw_Command *cmd, Draw_Vertex *v0, Draw_Vertex *v1, Draw_Vertex *v2) {
    Draw_AABB vertex_aabb = calculate_aabb_for_vertices(v0, v1, v2);
    Draw_AABB scissored_aabb = intersect_aabb(vertex_aabb, cmd->scissors);
    Draw_AABB aabb = intersect_aabb(scissored_aabb, cmd->frame_buffer->w, cmd->frame_buffer->h);
    
    for(f32 y = aabb.min.y; y <= aabb.max.y; ++y) {
        for(f32 x = aabb.min.x; x <= aabb.max.x; ++x) {
            v2f point = v2f(x, y);
            
            f32 s, t, u;
            if(!point_inside_triangle(point, v0->position, v1->position, v2->position, &s, &t, &u)) continue;

            v2i texel = v2i((s32) roundf(x), (s32) roundf(y));

            Color color = { 255, 255, 255, 255 };
            
            if(cmd->draw.options & DRAW_OPTION_Textured) {
                v2f interpolated_uv = interpolate(s, v0->uv, t, v1->uv, u, v2->uv);
                Color texture_color = query_texture(cmd->draw.texture, interpolated_uv);
                color = texture_color;
            }
            
            if(cmd->draw.options & DRAW_OPTION_Colored) {
                Color vertex_color = interpolate(s, v0->color, t, v1->color, u, v2->color);
                color = multiply(vertex_color, color);
            }

            if(cmd->draw.options & DRAW_OPTION_Blending) {
                color = mix(color, query_frame_buffer(cmd->frame_buffer, texel.x, texel.y));
            }

            write_frame_buffer(cmd->frame_buffer, texel.x, texel.y, color);
        }
    }
}

static
void execute_draw_command(Draw_Command *cmd) {
    switch(cmd->kind) {
        case DRAW_COMMAND_Clear: {
            u8 channels = channels_per_pixel[cmd->frame_buffer->format];
            u8 color_array[4];
            color_array[channel_indices[cmd->frame_buffer->format][CHANNEL_R]] = cmd->clear.color.r;
            color_array[channel_indices[cmd->frame_buffer->format][CHANNEL_G]] = cmd->clear.color.g;
            color_array[channel_indices[cmd->frame_buffer->format][CHANNEL_B]] = cmd->clear.color.b;
            color_array[channel_indices[cmd->frame_buffer->format][CHANNEL_A]] = cmd->clear.color.a;
            
            Draw_AABB aabb = intersect_aabb(cmd->scissors, cmd->frame_buffer->w, cmd->frame_buffer->h);
            
            for(f32 y = aabb.min.y; y <= aabb.max.y; y += 1) {
                for(f32 x = aabb.min.x; x <= aabb.max.x; x += 1) {
                    u8 *pixel = get_pixel_in_frame_buffer(cmd->frame_buffer, (s32) x, (s32) y);
                    memcpy(pixel, color_array, channels);
                }
            }
        } break;
        
        case DRAW_COMMAND_Draw: {
            for(s64 i = 0; i < cmd->draw.vertex_count; i += 3) {
                draw_triangle(cmd, &cmd->draw.vertices[i], &cmd->draw.vertices[i + 1], &cmd->draw.vertices[i + 2]);
            }
        } break;
    }
}

static
void flush_draw_commands() {
    for(Draw_Command &cmd : state.commands) {
        execute_draw_command(&cmd);
        destroy_draw_command(&cmd);
    }
    
    state.commands.clear();
}



/* -------------------------------------------- Software Renderer -------------------------------------------- */

static
void create_back_buffer() {
    Color_Format format;

#if FOUNDATION_WIN32
    format = COLOR_FORMAT_BGRA;
#else
    format = COLOR_FORMAT_RGBA;
#endif
    
    create_frame_buffer(&state.back_buffer, state.window->w, state.window->h, format);
}

static
void destroy_back_buffer() {
    destroy_frame_buffer(&state.back_buffer);
}

void create_software_renderer(Window *window) {
    state.window = window;
    state.bound_frame_buffer = &state.back_buffer;
    state.bound_scissors     = { v2f(0, 0), v2f((f32) window->w, (f32) window->h) };
    create_back_buffer();
}

void destroy_software_renderer() {
    destroy_back_buffer();
    state.window = null;
}

void maybe_resize_back_buffer() {
    if(!state.window->resized_this_frame) return;

    destroy_back_buffer();
    create_back_buffer();
}



/* ------------------------------------------------- Colors ------------------------------------------------- */

Color lerp(Color lhs, Color rhs, f32 t) {
    return mix(lhs, rhs, 1.0f - t, t);
}



/* ------------------------------------------------- Texture ------------------------------------------------- */

static
b8 texture_is_valid_for_draw(Texture *texture) {
    return texture->buffer != 0 && texture->w > 0 && texture->h > 0 && texture->format != COLOR_FORMAT_Unknown;
}

Error_Code create_texture_from_file(Texture *texture, string file_path) {
    char *cstring = to_cstring(Default_Allocator, file_path);
    defer { free_cstring(Default_Allocator, cstring); };

    int channels;
    texture->buffer = stbi_load(cstring, (int *) &texture->w, (int *) &texture->h, &channels, 0);

    if(texture->buffer == 0) {
        texture->w = 0;
        texture->h = 0;
        return ERROR_File_Not_Found;
    }
    
    switch(channels) {
    case 1: texture->format = COLOR_FORMAT_R;    break;
    case 2: texture->format = COLOR_FORMAT_RG;   break;
    case 3: texture->format = COLOR_FORMAT_RGB;  break;
    case 4: texture->format = COLOR_FORMAT_RGBA; break;
    }

    return Success;
}

void create_texture_from_memory(Texture *texture, u8 *buffer, s32 w, s32 h, u8 channels) {
    create_texture_from_memory(texture, buffer, w, h, get_color_format_for_channels(channels));
}

void create_texture_from_memory(Texture *texture, u8 *buffer, s32 w, s32 h, Color_Format format) {
    texture->w = w;
    texture->h = h;
    texture->format = format;
    
    s64 bytes = (s64) texture->w * (s64) texture->h * (s64) channels_per_pixel[texture->format];
    texture->buffer = (u8 *) Default_Allocator->allocate(bytes);
    memcpy(texture->buffer, buffer, bytes);
}

void destroy_texture(Texture *texture) {
    Default_Allocator->deallocate(texture->buffer); // stbi_load uses the Default_Allocator thanks to the macros in foundation.cpp
    texture->buffer = null;
    texture->w      = 0;
    texture->h      = 0;
    texture->format = COLOR_FORMAT_Unknown;
}



/* ----------------------------------------------- Frame Buffer ----------------------------------------------- */

void create_frame_buffer(Frame_Buffer *frame_buffer, s32 w, s32 h, u8 channels) {
    create_frame_buffer(frame_buffer, w, h, get_color_format_for_channels(channels));
}

void create_frame_buffer(Frame_Buffer *frame_buffer, s32 w, s32 h, Color_Format format) {
    frame_buffer->w      = w;
    frame_buffer->h      = h;
    frame_buffer->format = format;
    frame_buffer->buffer = (u8 *) Default_Allocator->allocate((s64) frame_buffer->w * (s64) frame_buffer->h * (s64) channels_per_pixel[frame_buffer->format]);
}

void destroy_frame_buffer(Frame_Buffer *frame_buffer) {
    Default_Allocator->deallocate(frame_buffer->buffer);
    frame_buffer->buffer = null;
    frame_buffer->w      = 0;
    frame_buffer->h      = 0;
    frame_buffer->format = COLOR_FORMAT_Unknown;
}

void bind_frame_buffer(Frame_Buffer *frame_buffer) {
    state.bound_frame_buffer = frame_buffer;
}

void unbind_frame_buffer() {
    state.bound_frame_buffer = &state.back_buffer;
}

void blit_frame_buffer(Frame_Buffer *dst, Frame_Buffer *src) {
    //
    // Flush all remaining commands
    //
    flush_draw_commands();

    //
    // Convert the specificed source buffer into the internal window format (which is BGRA on windows).
    //
    s32 w = min(src->w, dst->w);
    s32 h = min(src->h, dst->h);
    for(s32 y = 0; y < h; ++y) {
        for(s32 x = 0; x < w; ++x) {
            Color color = query_frame_buffer(src, x, y);
            write_frame_buffer(dst, x, y, color);
        }
    }
}

void blit_frame_buffer(Frame_Buffer *src) {
    blit_frame_buffer(&state.back_buffer, src);
}

void swap_buffers(Frame_Buffer *src) {
    blit_frame_buffer(&state.back_buffer, src);
    swap_buffers();
}

void swap_buffers() {
    //
    // Flush all remaining commands
    //
    flush_draw_commands();

    //
    // Actually blit the pixels onto the window.
    //
    if(state.window->w > 0 && state.window->h > 0 && state.back_buffer.w > 0 && state.back_buffer.h > 0) {
        blit_pixels_to_window(state.window, state.back_buffer.buffer, state.back_buffer.w, state.back_buffer.h, channels_per_pixel[state.back_buffer.format]);
    }
}


void write_frame_buffer(Frame_Buffer *frame_buffer, s32 x, s32 y, Color color) {
    if(x < 0 || x >= frame_buffer->w || y < 0 || y >= frame_buffer->h) return;

    u8 channels = channels_per_pixel[frame_buffer->format];
    
    u8 *pixel = get_pixel_in_frame_buffer(frame_buffer, x, y);
    if(channels > 0) *(pixel + channel_indices[frame_buffer->format][CHANNEL_R]) = color.r;
    if(channels > 1) *(pixel + channel_indices[frame_buffer->format][CHANNEL_G]) = color.g;
    if(channels > 2) *(pixel + channel_indices[frame_buffer->format][CHANNEL_B]) = color.b;
    if(channels > 3) *(pixel + channel_indices[frame_buffer->format][CHANNEL_A]) = color.a;
}

void write_frame_buffer(s32 x, s32 y, Color color) {
    write_frame_buffer(&state.back_buffer, x, y, color);
}

Color query_frame_buffer(Frame_Buffer *frame_buffer, s32 x, s32 y) {
    if(x < 0 || x >= frame_buffer->w || y < 0 || y >= frame_buffer->h) return Color(255, 255, 255, 255);

    u8 channels = channels_per_pixel[frame_buffer->format];

    u8 *pixel = get_pixel_in_frame_buffer(frame_buffer, x, y);
    
    return query_pixel(pixel, frame_buffer->format);
}

Color query_frame_buffer(s32 x, s32 y) {
    return query_frame_buffer(&state.back_buffer, x, y);
}



/* ---------------------------------------------- Draw Commands ---------------------------------------------- */

static
Draw_Command *make_quad_command(s32 x0, s32 y0, s32 x1, s32 y1) {
    Draw_Command *command = make_triangle_draw_command(6);
    command->draw.vertices[0].position = v2f((f32) x0, (f32) y0);
    command->draw.vertices[1].position = v2f((f32) x1, (f32) y1);
    command->draw.vertices[2].position = v2f((f32) x1, (f32) y0);
    command->draw.vertices[3].position = v2f((f32) x0, (f32) y0);
    command->draw.vertices[4].position = v2f((f32) x0, (f32) y1);
    command->draw.vertices[5].position = v2f((f32) x1, (f32) y1);
    return command;
}

void set_scissors(s32 x0, s32 y0, s32 x1, s32 y1) {
    state.bound_scissors = { v2f((f32) x0, (f32) y0), v2f((f32) x1, (f32) y1) };
}

void clear_scissors() {
    state.bound_scissors = { v2f(0, 0), v2f((f32) state.window->w, (f32) state.window->h) };
}

void clear_frame(Color color) {
    Draw_Command *command = make_draw_command(DRAW_COMMAND_Clear);
    command->clear.color = color;
}

void draw_quad(s32 x0, s32 y0, s32 x1, s32 y1, Color color0, Color color1, Color color2, Color color3) {
    Draw_Command *command = make_quad_command(x0, y0, x1, y1);
    command->draw.vertices[0].color = color0;
    command->draw.vertices[1].color = color2;
    command->draw.vertices[2].color = color1;
    command->draw.vertices[3].color = color0;
    command->draw.vertices[4].color = color3;
    command->draw.vertices[5].color = color2;
    command->draw.options = DRAW_OPTION_Colored | DRAW_OPTION_Blending;
}

void draw_quad(s32 x0, s32 y0, s32 x1, s32 y1, Color color) {
    draw_quad(x0, y0, x1, y1, color, color, color, color);
}

void draw_quad(s32 x0, s32 y0, s32 x1, s32 y1, Texture *texture) {
    Draw_Command *command = make_quad_command(x0, y0, x1, y1);
    command->draw.vertices[0].uv = v2f(0, 0);
    command->draw.vertices[1].uv = v2f(1, 1);
    command->draw.vertices[2].uv = v2f(1, 0);
    command->draw.vertices[3].uv = v2f(0, 0);
    command->draw.vertices[4].uv = v2f(0, 1);
    command->draw.vertices[5].uv = v2f(1, 1);
    command->draw.texture = texture;
    command->draw.options = texture_is_valid_for_draw(texture) ? DRAW_OPTION_Textured | DRAW_OPTION_Blending : DRAW_OPTION_Nothing;
}

void draw_outlined_quad(s32 x0, s32 y0, s32 x1, s32 y1, s32 thickness, Color color) {
    draw_quad(x0, y0, x1 - thickness, y0 + thickness, color);
    draw_quad(x1 - thickness, y0, x1, y1 - thickness, color);
    draw_quad(x0 + thickness, y1 - thickness, x1, y1, color);
    draw_quad(x0, y0 + thickness, x0 + thickness, y1, color);
}



/* ----------------------------------------------- Font Helpers ----------------------------------------------- */

Error_Code create_software_font_from_file(Software_Font *software_font, string file_path, s16 size, Glyph_Set glyphs_to_load) {
    software_font->underlying = Default_Allocator->New<Font>();
    Error_Code error_code = create_font_from_file(software_font->underlying, file_path, size, FONT_FILTER_Mono, glyphs_to_load);
    
    if(error_code == Success) {
        for(Font_Atlas *atlas = software_font->underlying->atlas; atlas != null; atlas = atlas->next) {
            Texture *texture = Default_Allocator->New<Texture>();

            assert(atlas->channels == 1 || atlas->channels == 4);
            Color_Format color_format = atlas->channels == 1 ? COLOR_FORMAT_A : COLOR_FORMAT_RGBA;
            create_texture_from_memory(texture, atlas->bitmap, atlas->w, atlas->h, color_format);
            atlas->user_handle = texture;
        }
    }
    
    return error_code;
}

void destroy_software_font(Software_Font *software_font) {
    for(Font_Atlas *atlas = software_font->underlying->atlas; atlas != null; atlas = atlas->next) {
        Texture *texture = (Texture *) atlas->user_handle;
        destroy_texture(texture);
        Default_Allocator->deallocate(texture);
    }
    
    destroy_font(software_font->underlying);
    Default_Allocator->deallocate(software_font->underlying);
}

void draw_text(Software_Font *software_font, string text, s32 x, s32 y, Text_Alignment alignment, Color color) {
    auto mesh = build_text_mesh(software_font->underlying, text, x, y, alignment, Default_Allocator);

    for(s64 i = 0; i < mesh.glyph_count; ++i) {
        Texture *texture = (Texture *) mesh.atlasses[i]->user_handle;

        Draw_Command *command = make_triangle_draw_command(6);
        command->draw.vertices[0].position = v2f(mesh.vertices[i * 12 + 0], mesh.vertices[i * 12 + 1]);
        command->draw.vertices[0].color = color;
        command->draw.vertices[0].uv = v2f(mesh.uvs[i * 12 + 0], mesh.uvs[i * 12 + 1]);
        command->draw.vertices[1].position = v2f(mesh.vertices[i * 12 + 2], mesh.vertices[i * 12 + 3]);
        command->draw.vertices[1].color = color;
        command->draw.vertices[1].uv = v2f(mesh.uvs[i * 12 + 2], mesh.uvs[i * 12 + 3]);
        command->draw.vertices[2].position = v2f(mesh.vertices[i * 12 + 4], mesh.vertices[i * 12 + 5]);
        command->draw.vertices[2].color = color;
        command->draw.vertices[2].uv = v2f(mesh.uvs[i * 12 + 4], mesh.uvs[i * 12 + 5]);
        command->draw.vertices[3].position = v2f(mesh.vertices[i * 12 + 6], mesh.vertices[i * 12 + 7]);
        command->draw.vertices[3].color = color;
        command->draw.vertices[3].uv = v2f(mesh.uvs[i * 12 + 6], mesh.uvs[i * 12 + 7]);
        command->draw.vertices[4].position = v2f(mesh.vertices[i * 12 + 8], mesh.vertices[i * 12 + 9]);
        command->draw.vertices[4].color = color;
        command->draw.vertices[4].uv = v2f(mesh.uvs[i * 12 + 8], mesh.uvs[i * 12 + 9]);
        command->draw.vertices[5].position = v2f(mesh.vertices[i * 12 + 10], mesh.vertices[i * 12 + 11]);
        command->draw.vertices[5].color = color;
        command->draw.vertices[5].uv = v2f(mesh.uvs[i * 12 + 10], mesh.uvs[i * 12 + 11]);
        command->draw.texture = texture;
        command->draw.options = texture_is_valid_for_draw(texture) ? (DRAW_OPTION_Colored | DRAW_OPTION_Textured | DRAW_OPTION_Blending) : DRAW_OPTION_Nothing;
    }
    
    deallocate_text_mesh(&mesh, Default_Allocator);
}
