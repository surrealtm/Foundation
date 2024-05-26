#pragma once

#include "foundation.h"
#include "strings.h"

//
// Forward declarations to avoid having to include the D3D11 headers here.
//

struct ID3D10Blob;
typedef ID3D10Blob ID3DBlob;

struct ID3D11Buffer;
struct ID3D11Texture2D;
struct ID3D11BlendState;
struct ID3D11PixelShader;
struct ID3D11InputLayout;
struct ID3D11SamplerState;
struct ID3D11VertexShader;
struct ID3D11RasterizerState;
struct ID3D11RenderTargetView;
struct ID3D11DepthStencilView;
struct ID3D11DepthStencilState;
struct ID3D11ShaderResourceView;

struct Window;

//
// D3D11-specific constants to avoid dynamic allocations and stuff.
//

#define D3D11_MAX_FRAMEBUFFER_COLOR_ATTACHMENTS 4
#define D3D11_MAX_VERTEX_BUFFERS                4
#define D3D11_MAX_SHADER_INPUTS                12

//
// Wrappers around the D3D11 internals for easier state manipulation.
//

enum Vertex_Buffer_Topology { // These mirror D3D11_PRIMITIVE_TOPOLOGY 
    VERTEX_BUFFER_Undefined   = 0x0,
    VERTEX_BUFFER_Lines       = 0x1,
    VERTEX_BUFFER_Line_Strips = 0x3,
    VERTEX_BUFFER_Triangles   = 0x4,
};

struct Vertex_Buffer {
    Vertex_Buffer_Topology topology;
    u8 dimensions;
    u64 vertex_count;
    u64 capacity; // D3D11 does not allow for dynamic resizing of a buffer. We can always use less than that initial size (using the vertex_count), but we can never grow it. This capacity marks that initial size in bytes.
    ID3D11Buffer *handle;
};

struct Vertex_Buffer_Array {
    Vertex_Buffer_Topology topology;
    Vertex_Buffer buffers[D3D11_MAX_VERTEX_BUFFERS];
    s64 count;
};

enum Texture_Hints {
    TEXTURE_FILTER_Nearest = 0x1,
    TEXTURE_FILTER_Linear  = 0x2,
    TEXTURE_FILTER         = 0x3,

    TEXTURE_WRAP_Repeat = 0x4,
    TEXTURE_WRAP_Edge   = 0x8,
    TEXTURE_WRAP_Border = 0x10,
    TEXTURE_WRAP        = 0x1c,
};

BITWISE(Texture_Hints);

struct Texture {
    s32 w, h;
    u8 channels;
    ID3D11Texture2D *handle;
    ID3D11SamplerState *sampler;
    ID3D11ShaderResourceView *view;
};

enum Shader_Type {
    SHADER_Vertex = 0x1,
    SHADER_Pixel  = 0x2,
};

BITWISE(Shader_Type);

struct Shader_Input_Specification {
    const char *name;
    u8 dimensions;
    u8 vertex_buffer_index;
};

struct Shader_Constant_Buffer {
    ID3D11Buffer *handle;
};

struct Shader {
    ID3DBlob *vertex_blob;
    ID3DBlob *pixel_blob;
    ID3D11VertexShader *vertex_shader;
    ID3D11PixelShader *pixel_shader;
    ID3D11InputLayout *input_layout;
};

struct Frame_Buffer_Attachment {
    s32 w, h;
    u32 format; // DXGI_FORMAT
    ID3D11RenderTargetView *view;
    ID3D11Texture2D *texture;
};

struct Frame_Buffer {
    s8 samples;

    Frame_Buffer_Attachment colors[D3D11_MAX_FRAMEBUFFER_COLOR_ATTACHMENTS];
    s64 color_count;
    
    u32 depth_format; // DXGI_FORMAT
    ID3D11Texture2D *depth_stencil_texture;
    ID3D11DepthStencilView *depth_stencil_view;
    ID3D11DepthStencilState *depth_stencil_state;
    b8 has_depth;
};

enum Blend_Mode {
    BLEND_Disabled,
    BLEND_Default,
    BLEND_Additive,
};

struct Pipeline_State {
    // --- User Level Input
    b8 enable_culling     = true;
    b8 enable_depth_test  = true;
    b8 enable_scissors    = false;
    b8 enable_multisample = false;
    Blend_Mode blend_mode = BLEND_Default;
    
    // --- Internal Handle
    ID3D11RasterizerState *rasterizer;
    ID3D11BlendState *blender;
};

void create_d3d11_context(Window *window);
void destroy_d3d11_context(Window *window);
void swap_d3d11_buffers(Window *window);
Frame_Buffer *get_default_frame_buffer(Window *window);

void create_vertex_buffer(Vertex_Buffer *buffer, f32 *data, u64 float_count, u8 dimensions, Vertex_Buffer_Topology topology, b8 allow_updates = false);
void allocate_vertex_buffer(Vertex_Buffer *buffer, u64 float_count, u8 dimensions, Vertex_Buffer_Topology topology);
void destroy_vertex_buffer(Vertex_Buffer *buffer);
void update_vertex_buffer(Vertex_Buffer *buffer, f32 *data, u64 float_count);
void bind_vertex_buffer(Vertex_Buffer *buffer);
void draw_vertex_buffer(Vertex_Buffer *buffer);

void create_vertex_buffer_array(Vertex_Buffer_Array *array, Vertex_Buffer_Topology topology);
void destroy_vertex_buffer_array(Vertex_Buffer_Array *array);
void add_vertex_data(Vertex_Buffer_Array *array, f32 *data, u64 float_count, u8 dimensions, b8 allow_updates = false);
void allocate_vertex_data(Vertex_Buffer_Array *array, u64 float_count, u8 dimensions);
void update_vertex_data(Vertex_Buffer_Array *array, s64 index, f32 *data, u64 float_count);
void bind_vertex_buffer_array(Vertex_Buffer_Array *array);
void draw_vertex_buffer_array(Vertex_Buffer_Array *array);

void create_texture_from_file(Texture *texture, string file_path, Texture_Hints hints);
void create_texture_from_memory(Texture *texture, u8 *buffer, s32 w, s32 h, u8 channels, Texture_Hints hints);
void destroy_texture(Texture *texture);
void bind_texture(Texture *texture, s64 index_in_shader);

void create_shader_constant_buffer(Shader_Constant_Buffer *buffer, s64 index_in_shader, s64 size_in_bytes, void *initial_data = null);
void destroy_shader_constant_buffer(Shader_Constant_Buffer *buffer);
void update_shader_constant_buffer(Shader_Constant_Buffer *buffer, void *data);
void bind_shader_constant_buffer(Shader_Constant_Buffer *buffer, s64 index_in_shader, Shader_Type shader_types);

void create_shader_from_file(Shader *shader, string file_path, Shader_Input_Specification *inputs, s64 input_count);
void destroy_shader(Shader *shader);
void bind_shader(Shader *shader);

void create_frame_buffer(Frame_Buffer *frame_buffer, u8 samples = 1);
void destroy_frame_buffer(Frame_Buffer *frame_buffer);
void create_frame_buffer_color_attachment(Frame_Buffer *frame_buffer, s32 w, s32 h, b8 hdr = false);
void create_frame_buffer_depth_stencil_attachment(Frame_Buffer *frame_buffer, s32 w, s32 h);
void bind_frame_buffer(Frame_Buffer *frame_buffer);
void clear_frame_buffer(Frame_Buffer *frame_buffer, f32 r, f32 g, f32 b);
void blit_frame_buffer(Frame_Buffer *dst, Frame_Buffer *src);

void create_pipeline_state(Pipeline_State *state); // User Level Input must be set before this!
void destroy_pipeline_state(Pipeline_State *state);
void bind_pipeline_state(Pipeline_State *state);
