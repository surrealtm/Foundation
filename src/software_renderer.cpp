#include "software_renderer.h"
#include "memutils.h"
#include "math/v2.h"
#include "math/v4.h"
#include "window.h"

#include "Dependencies/stb_image.h"

enum Draw_Command_Kind {
    DRAW_COMMAND_Nothing,
    DRAW_COMMAND_Clear,
    DRAW_COMMAND_Triangles,
};

enum Draw_Command_Options {
    DRAW_OPTION_Nothing  = 0x0,
    DRAW_OPTION_Colored  = 0x1,
    DRAW_OPTION_Textured = 0x2,
};

BITWISE(Draw_Command_Options);

struct Draw_AABB {
    v2i min, max;
};

struct Draw_Vertex {
    v4i position;
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
        } triangles;
    };
};

struct Software_Renderer {
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
void include_vertex_in_aabb(Draw_AABB *aabb, Draw_Vertex *vertex) {
    aabb->min.x = min(aabb->min.x, vertex->position.x);
    aabb->min.y = min(aabb->min.y, vertex->position.y);
    aabb->max.x = max(aabb->max.x, vertex->position.x);
    aabb->max.y = max(aabb->max.y, vertex->position.y);
}

static
Draw_AABB calculate_aabb_for_vertices(Draw_Vertex *v0, Draw_Vertex *v1, Draw_Vertex *v2) {
    Draw_AABB aabb;
    aabb.min = v2i(MAX_S32, MAX_S32);
    aabb.max = v2i(MIN_S32, MIN_S32);
    include_vertex_in_aabb(&aabb, v0);
    include_vertex_in_aabb(&aabb, v1);
    include_vertex_in_aabb(&aabb, v2);
    return aabb;
}

static
b8 point_inside_triangle(const v4i &p, const v4i &p0, const v4i &p1, const v4i &p2, f32 *s, f32 *t, f32 *u) {
    f32 A = (-p1.y * p2.x + p0.y * (-p1.x + p2.x) + p0.x * (p1.y - p2.y) + p1.x * p2.y) / 2.f;
    f32 sign = (A < 0) ? -1.f : 1.f;
    f32 _s = (p0.y * p2.x - p0.x * p2.y + (p2.y - p0.y) * p.x + (p0.x - p2.x) * p.y) * sign;
    f32 _t = (p0.x * p1.y - p0.y * p1.x + (p0.y - p1.y) * p.x + (p1.x - p0.x) * p.y) * sign;
    
    f32 denom = 2.f * A * sign;
    
    b8 inside_triangle = _s >= 0 && _t >= 0 && (_s + _t) < denom;
    if(!inside_triangle) return false;
    
    *s = _s / denom;
    *t = _t / denom;
    *u = 1.f - *s - *t;
    
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
    Draw_Command *command = make_draw_command(DRAW_COMMAND_Triangles);
    command->triangles.vertex_count = vertex_count;
    command->triangles.vertices     = (Draw_Vertex *) Default_Allocator->allocate(command->triangles.vertex_count * sizeof(Draw_Vertex));
    return command;
}

static
void draw_triangle(Draw_Command *cmd, Draw_Vertex *v0, Draw_Vertex *v1, Draw_Vertex *v2) {
    Draw_AABB aabb = calculate_aabb_for_vertices(v0, v1, v2);
    u8 color_array[4];
    
    for(s32 y = aabb.min.y; y <= aabb.max.y; ++y) {
        for(s32 x = aabb.min.x; x <= aabb.max.x; ++x) {
            v2i point = v2i(x, y);
            
            f32 s, t, u;
            if(!point_inside_triangle(v4i(point.x, point.y, 0, 1), v0->position, v1->position, v2->position, &s, &t, &u)) continue;
            
            if(cmd->triangles.options & DRAW_OPTION_Colored) {
                Color interpolated_color = interpolate(u, v0->color, s, v1->color, t, v2->color);
                color_array[0] = interpolated_color.r;
                color_array[1] = interpolated_color.g;
                color_array[2] = interpolated_color.b;
                color_array[3] = interpolated_color.a;
            } else {
                color_array[0] = color_array[1] = color_array[2] = color_array[3] = 255;
            }
            
            u8 *pixel = get_pixel_in_frame_buffer(cmd->frame_buffer, x, y);
            memcpy(pixel, color_array, cmd->frame_buffer->channels);
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
        
        case DRAW_COMMAND_Triangles: {
            for(s64 i = 0; i < cmd->triangles.vertex_count; i += 3) {
                draw_triangle(cmd, &cmd->triangles.vertices[i], &cmd->triangles.vertices[i + 1], &cmd->triangles.vertices[i + 2]);
            }
        } break;
    }
}

static
void flush_draw_commands() {
    for(Draw_Command &cmd : state.commands) {
        execute_draw_command(&cmd);
    }
    
    state.commands.clear();
}



/* ------------------------------------------------- Texture ------------------------------------------------- */

void create_texture_from_file(Texture *texture, string file_path) {
    char *cstring = to_cstring(Default_Allocator, file_path);
    defer { free_cstring(Default_Allocator, cstring); };
    
    texture->buffer = stbi_load(cstring, (int *) &texture->w, (int *) &texture->h, (int *) &texture->channels, 0);
}

void destroy_texture(Texture *texture) {
    stbi_image_free(texture->buffer);
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

void swap_buffers(Window *dst, Frame_Buffer *src) {
    flush_draw_commands();
    
    u8 *bgra = convert_rgba_to_bgra(src->buffer, src->w, src->h);
    blit_pixels_to_window(dst, bgra, src->w, src->h);
    Default_Allocator->deallocate(bgra);
}



/* ----------------------------------------------- Draw Commands ----------------------------------------------- */

void clear_frame(Color color) {
    Draw_Command *command = make_draw_command(DRAW_COMMAND_Clear);
    command->clear.color = color;
}

void draw_quad(s32 x0, s32 y0, s32 x1, s32 y1, Color color0, Color color1, Color color2, Color color3) {
    Draw_Command *command = make_triangle_draw_command(6);
    command->triangles.vertices[0].position = v4i(x0, y0, 0, 1);
    command->triangles.vertices[0].color    = color0;
    command->triangles.vertices[1].position = v4i(x1, y1, 0, 1);
    command->triangles.vertices[1].color    = color2;
    command->triangles.vertices[2].position = v4i(x1, y0, 0, 1);
    command->triangles.vertices[2].color    = color1;
    command->triangles.vertices[3].position = v4i(x0, y0, 0, 1);
    command->triangles.vertices[3].color    = color0;
    command->triangles.vertices[4].position = v4i(x0, y1, 0, 1);
    command->triangles.vertices[4].color    = color3;
    command->triangles.vertices[5].position = v4i(x1, y1, 0, 1);
    command->triangles.vertices[5].color    = color2;
    command->triangles.options = DRAW_OPTION_Colored;
}

void draw_quad(s32 x0, s32 y0, s32 x1, s32 y1, Color color) {
    draw_quad(x0, y0, x1, y1, color, color, color, color);
}