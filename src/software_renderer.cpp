#include "software_renderer.h"
#include "memutils.h"
#include "math/v2.h"
#include "math/v4.h"
#include "window.h"

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
    u8 *internal_back_buffer;
    u8 internal_back_buffer_channels;
    
    Frame_Buffer *bound_frame_buffer = null;
    
    b8 command_system_setup = false;
    Resizable_Array<Draw_Command> commands;
};

static thread_local Software_Renderer state;

/* -------------------------------------------------- Helpers -------------------------------------------------- */

static
u8 *get_pixel_in_frame_buffer(Frame_Buffer *frame_buffer, s32 x, s32 y) {
    s64 offset = ((s64) y * (s64) frame_buffer->w + x) * frame_buffer->channels;
    return &frame_buffer->buffer[offset];
}

static
u8 *get_pixel_in_texture(Texture *texture, s32 x, s32 y) {
    s64 offset = ((s64) y * (s64) texture->w + x) * texture->channels;
    return &texture->buffer[offset];
}

static
Color query_frame_buffer(Frame_Buffer *frame_buffer, s32 x, s32 y) {
    if(x < 0 || x >= frame_buffer->w || y < 0 || y >= frame_buffer->h) return Color(255, 255, 255, 255);

    u8 *pixel = get_pixel_in_frame_buffer(frame_buffer, x, y);
    Color result;
    result.r = frame_buffer->channels > 0 ? *(pixel + 0) : 255;
    result.g = frame_buffer->channels > 1 ? *(pixel + 1) : 255;
    result.b = frame_buffer->channels > 2 ? *(pixel + 2) : 255;
    result.a = frame_buffer->channels > 3 ? *(pixel + 3) : 255;
    return result;
}

static
Color query_texture(Texture *texture, v2f uv) {
    u8 *pixel = get_pixel_in_texture(texture, (s32) (uv.x * texture->w), (s32) (uv.y * texture->h));
    Color result;
    result.r = texture->channels > 0 ? *(pixel + 0) : 255;
    result.g = texture->channels > 1 ? *(pixel + 1) : 255;
    result.b = texture->channels > 2 ? *(pixel + 2) : 255;
    result.a = texture->channels > 3 ? *(pixel + 3) : 255;
    return result;
}

static
void include_vertex_in_aabb(Draw_AABB *aabb, Draw_Vertex *vertex) {
    aabb->min.x = min(aabb->min.x, vertex->position.x);
    aabb->min.y = min(aabb->min.y, vertex->position.y);
    aabb->max.x = max(aabb->max.x, vertex->position.x);
    aabb->max.y = max(aabb->max.y, vertex->position.y);
}

static
Draw_AABB calculate_aabb_for_vertices(Frame_Buffer *frame_buffer, Draw_Vertex *v0, Draw_Vertex *v1, Draw_Vertex *v2) {
    Draw_AABB aabb;

    aabb.min = v2f(MAX_F32, MAX_F32);
    aabb.max = v2f(MIN_F32, MIN_F32);

    include_vertex_in_aabb(&aabb, v0);
    include_vertex_in_aabb(&aabb, v1);
    include_vertex_in_aabb(&aabb, v2);

    // Don't bother trying to render pixels outside of the frame buffer's area.
    aabb.min.x = max(aabb.min.x, 0);
    aabb.min.y = max(aabb.min.y, 0);
    aabb.max.x = min(aabb.max.x, (f32) frame_buffer->w - 1);
    aabb.max.y = min(aabb.max.y, (f32) frame_buffer->h - 1);
    
    return aabb;
}

static
b8 point_inside_triangle(const v2f &p, const v2f &p0, const v2f &p1, const v2f &p2, f32 *s, f32 *t, f32 *u) {
    f32 A = (-p1.y * p2.x + p0.y * (-p1.x + p2.x) + p0.x * (p1.y - p2.y) + p1.x * p2.y) / 2.f;
    f32 sign = (A < 0) ? -1.f : 1.f;
    f32 _t = (p0.y * p2.x - p0.x * p2.y + (p2.y - p0.y) * p.x + (p0.x - p2.x) * p.y) * sign;
    f32 _u = (p0.x * p1.y - p0.y * p1.x + (p0.y - p1.y) * p.x + (p1.x - p0.x) * p.y) * sign;
    
    f32 denom = 2.f * A * sign;
    
    b8 inside_triangle = _t >= 0 && _u >= 0 && (_t + _u) < denom;
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



/* ------------------------------------------------ Draw Command ------------------------------------------------ */

static
Draw_Command *make_draw_command(Draw_Command_Kind kind) {
    if(!state.command_system_setup) {
        state.commands.allocator = Default_Allocator;
        state.command_system_setup = true;
    }
    
    Draw_Command *command = state.commands.push();
    command->kind = kind;
    command->frame_buffer = state.bound_frame_buffer;
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
    Draw_AABB aabb = calculate_aabb_for_vertices(cmd->frame_buffer, v0, v1, v2);
    
    for(f32 y = aabb.min.y; y <= aabb.max.y; ++y) {
        for(f32 x = aabb.min.x; x <= aabb.max.x; ++x) {
            v2f point = v2f(x, y);
            
            f32 s, t, u;
            if(!point_inside_triangle(point, v0->position, v1->position, v2->position, &s, &t, &u)) continue;

            Color source_color;
            
            if(cmd->draw.options & DRAW_OPTION_Textured) {
                v2f interpolated_uv = interpolate(s, v0->uv, t, v1->uv, u, v2->uv);
                source_color = query_texture(cmd->draw.texture, interpolated_uv);
            }
            
            if(cmd->draw.options & DRAW_OPTION_Colored) {
                source_color = interpolate(s, v0->color, t, v1->color, u, v2->color);
            }

            if(cmd->draw.options & DRAW_OPTION_Blending) {
                source_color = mix(source_color, query_frame_buffer(cmd->frame_buffer, (s32) x, (s32) y));
            }
            
            u8 *output = get_pixel_in_frame_buffer(cmd->frame_buffer, (s32) x, (s32) y);
            memcpy(output, &source_color, cmd->frame_buffer->channels);
        }
    }
}

static
void execute_draw_command(Draw_Command *cmd) {
    switch(cmd->kind) {
        case DRAW_COMMAND_Clear: {
            u8 color_array[4] = { cmd->clear.color.r, cmd->clear.color.g, cmd->clear.color.b, cmd->clear.color.a };
            
            for(s32 y = 0; y < cmd->frame_buffer->h; ++y) {
                for(s32 x = 0; x < cmd->frame_buffer->w; ++x) {
                    u8 *pixel = get_pixel_in_frame_buffer(cmd->frame_buffer, x, y);
                    memcpy(pixel, color_array, cmd->frame_buffer->channels);
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
    state.internal_back_buffer_channels = 4;
    state.internal_back_buffer = (u8 *) Default_Allocator->allocate(state.window->w * state.window->h * state.internal_back_buffer_channels);   
}

static
void maybe_resize_software_renderer() {
    if(!state.window->resized_this_frame) return;

    Default_Allocator->deallocate(state.internal_back_buffer);
    create_back_buffer();
}

void create_software_renderer(Window *window) {
    state.window = window;
    create_back_buffer();
}

void destroy_software_renderer() {
    Default_Allocator->deallocate(state.internal_back_buffer);
    state.internal_back_buffer = null;
}



/* ------------------------------------------------- Texture ------------------------------------------------- */

static
b8 texture_is_valid_for_draw(Texture *texture) {
    return texture->buffer != 0 && texture->w > 0 && texture->h > 0 && texture->channels > 0;
}

void create_texture_from_file(Texture *texture, string file_path) {
    char *cstring = to_cstring(Default_Allocator, file_path);
    defer { free_cstring(Default_Allocator, cstring); };
    
    texture->buffer = stbi_load(cstring, (int *) &texture->w, (int *) &texture->h, (int *) &texture->channels, 0);
}

void create_texture_from_memory(Texture *texture, s32 w, s32 h, u8 channels, u8 *buffer) {
    texture->w = w;
    texture->h = h;
    texture->channels = channels;
    
    s64 bytes = (s64) texture->w * (s64) texture->h * (s64) texture->channels;
    texture->buffer = (u8 *) Default_Allocator->allocate(bytes);
    memcpy(texture->buffer, buffer, bytes);
}

void destroy_texture(Texture *texture) {
    Default_Allocator->deallocate(texture->buffer); // stbi_load uses the Default_Allocator thanks to the macros in foundation.cpp
    texture->buffer   = null;
    texture->w        = 0;
    texture->h        = 0;
    texture->channels = 0;
}



/* ----------------------------------------------- Frame Buffer ----------------------------------------------- */

void create_frame_buffer(Frame_Buffer *frame_buffer, s32 w, s32 h, u8 channels) {
    frame_buffer->w        = w;
    frame_buffer->h        = h;
    frame_buffer->channels = channels;
    frame_buffer->buffer   = (u8 *) Default_Allocator->allocate((s64) frame_buffer->w * (s64) frame_buffer->h * (s64) frame_buffer->channels);
}

void destroy_frame_buffer(Frame_Buffer *frame_buffer) {
    Default_Allocator->deallocate(frame_buffer->buffer);
    frame_buffer->buffer   = null;
    frame_buffer->w        = 0;
    frame_buffer->h        = 0;
    frame_buffer->channels = 0;
}

void bind_frame_buffer(Frame_Buffer *frame_buffer) {
    state.bound_frame_buffer = frame_buffer;
}

void swap_buffers(Frame_Buffer *src) {
    //
    // Flush all remaining commands
    //
    flush_draw_commands();

    //
    // Convert the specificed source buffer into the internal window format (which is BGRA on windows).
    //
    maybe_resize_software_renderer();
    s32 w = min(src->w, state.window->w);
    s32 h = min(src->h, state.window->h);
    for(s32 y = 0; y < h; ++y) {
        for(s32 x = 0; x < w; ++x) {
            Color color = query_frame_buffer(src, x, y);
            s64 offset = (y * w + x) * state.internal_back_buffer_channels;
            state.internal_back_buffer[offset + 0] = color.b;
            state.internal_back_buffer[offset + 1] = color.g;
            state.internal_back_buffer[offset + 2] = color.r;
        }
    }
    
    //
    // Actually blit the pixels onto the window.
    //
    blit_pixels_to_window(state.window, state.internal_back_buffer, src->w, src->h, state.internal_back_buffer_channels);
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
