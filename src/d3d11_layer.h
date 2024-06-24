#pragma once

#include "foundation.h"
#include "string_type.h"
#include "error.h"

//
// :TextureCompression
// This D3D11 abstraction layer supports the creation of compressed textures,
// that is, loading compressed data into a texture object. It does not support
// converting raw texture data into compressed data, since that is a little more
// involved.
// This layer provides procedures that load compressed textures from disk or from
// a buffer. It expects a certain data layout here for convenience. If that layout
// is not provided, the user must manually parse and pass this information (texture
// width / height...). The layout inside the buffer is essentially the following:
//    s32 width;
//    s32 height;
//    s32 channels;
//    u8 data[...];
//

//
// :ShaderCompilation
// Similar to :TextureCompression, we allow for offline shader compilation. The user
// can then just pass in the compiled shader binary, which should increase load
// performance. The layout for compiled shader output is the the following:
//    s64 vertex_size_in_bytes;
//    s64 pixel_size_in_bytes;
//    void vertex_blob[...];
//    void pixel_blob[...];
//

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
    TEXTURE_HINT_None = 0x0,

    TEXTURE_FILTER_Nearest            = 0x1,
    TEXTURE_FILTER_Linear             = 0x2,
    TEXTURE_FILTER_Comparison_Nearest = 0x4,
    TEXTURE_FILTER_Comparison_Linear  = 0x8,
    TEXTURE_FILTER                    = TEXTURE_FILTER_Nearest | TEXTURE_FILTER_Linear | TEXTURE_FILTER_Comparison_Nearest | TEXTURE_FILTER_Comparison_Linear,

    TEXTURE_WRAP_Repeat = 0x10,
    TEXTURE_WRAP_Edge   = 0x20,
    TEXTURE_WRAP_Border = 0x40,
    TEXTURE_WRAP        = TEXTURE_WRAP_Repeat | TEXTURE_WRAP_Edge | TEXTURE_WRAP_Border,

    TEXTURE_COMPRESS_BC7      = 0x80,
    TEXTURE_COMPRESS          = TEXTURE_COMPRESS_BC7,

    TEXTURE_Is_In_Srgb = 0x100,
};

BITWISE(Texture_Hints);

struct Border_Color {
    u8 r, g, b, a;
};

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
    s64 size_in_bytes;
    ID3D11Buffer *handle;
};

struct Compiled_Shader_Output {
    Error_Code error;
    void *vertex_blob;
    s64 vertex_size_in_bytes;
    void *pixel_blob;
    s64 pixel_size_in_bytes;
};

struct Shader {
    ID3D11VertexShader *vertex_shader;
    ID3D11PixelShader *pixel_shader;
    ID3D11InputLayout *input_layout;
};

enum Frame_Buffer_Color_Format {
    FRAME_BUFFER_COLOR_rgba8,
    FRAME_BUFFER_COLOR_rgba16,
    FRAME_BUFFER_COLOR_rgba32,
};

struct Frame_Buffer_Color_Attachment {
    b8 create_shader_view;
    s32 w, h;
    u32 format; // DXGI_FORMAT
    Texture_Hints shader_view_hints;
    Border_Color border_color;
    ID3D11RenderTargetView *render_view;
    ID3D11Texture2D *texture;
    ID3D11ShaderResourceView *shader_view;
    ID3D11SamplerState *sampler;
};

struct Frame_Buffer_Depth_Attachment {
    b8 create_shader_view;
    s32 w, h;
    u32 format; // DXGI_FORMAT
    Texture_Hints shader_view_hints;
    Border_Color border_color;
    ID3D11Texture2D *texture;
    ID3D11DepthStencilView *render_view;
    ID3D11DepthStencilState *state;
    ID3D11ShaderResourceView *shader_view;
    ID3D11SamplerState *sampler;
};

struct Frame_Buffer {
    s8 samples;

    Frame_Buffer_Color_Attachment colors[D3D11_MAX_FRAMEBUFFER_COLOR_ATTACHMENTS];
    s64 color_count;
    
    Frame_Buffer_Depth_Attachment depth;
    b8 has_depth;
};

enum Blend_Mode {
    BLEND_Disabled,
    BLEND_Default,
    BLEND_Additive,
};

enum Cull_Mode {
    CULL_Disabled,
    CULL_Back_Faces,
    CULL_Front_Faces,
};

struct Pipeline_State {
    // --- User Level Input
    Cull_Mode cull_mode   = CULL_Back_Faces;
    b8 enable_depth_test  = true;
    b8 enable_scissors    = false;
    b8 enable_multisample = false;
    Blend_Mode blend_mode = BLEND_Default;
    
    // --- Internal Handle
    ID3D11RasterizerState *rasterizer;
    ID3D11BlendState *blender;
};



/* ---------------------------------------------- Context Setup ---------------------------------------------- */

void create_d3d11_context(Window *window);
void destroy_d3d11_context(Window *window);
void swap_d3d11_buffers(Window *window);
void set_d3d11_fullscreen(Window *window, b8 fullscreen);
void clear_d3d11_state();
Frame_Buffer *get_default_frame_buffer(Window *window);
void resize_default_frame_buffer(Window *window);



/* ---------------------------------------------- Vertex Buffer ---------------------------------------------- */

void create_vertex_buffer(Vertex_Buffer *buffer, f32 *data, u64 float_count, u8 dimensions, Vertex_Buffer_Topology topology, b8 allow_updates = false);
void allocate_vertex_buffer(Vertex_Buffer *buffer, u64 float_count, u8 dimensions, Vertex_Buffer_Topology topology);
void destroy_vertex_buffer(Vertex_Buffer *buffer);
void update_vertex_buffer(Vertex_Buffer *buffer, f32 *data, u64 float_count);
void bind_vertex_buffer(Vertex_Buffer *buffer);
void draw_vertex_buffer(Vertex_Buffer *buffer);



/* ------------------------------------------- Vertex Buffer Array ------------------------------------------- */

void create_vertex_buffer_array(Vertex_Buffer_Array *array, Vertex_Buffer_Topology topology);
void destroy_vertex_buffer_array(Vertex_Buffer_Array *array);
void add_vertex_data(Vertex_Buffer_Array *array, f32 *data, u64 float_count, u8 dimensions, b8 allow_updates = false);
void allocate_vertex_data(Vertex_Buffer_Array *array, u64 float_count, u8 dimensions);
void update_vertex_data(Vertex_Buffer_Array *array, s64 index, f32 *data, u64 float_count);
void bind_vertex_buffer_array(Vertex_Buffer_Array *array);
void draw_vertex_buffer_array(Vertex_Buffer_Array *array);



/* ------------------------------------------------- Texture ------------------------------------------------- */

Error_Code create_texture_from_file(Texture *texture, string file_path, Texture_Hints hints, Border_Color border_color = {});
Error_Code create_texture_from_compressed_file(Texture *texture, string file_path, Texture_Hints hints, Border_Color border_color = {}); // :TextureCompression
Error_Code create_texture_from_compressed_memory(Texture *texture, string file_content, Texture_Hints hints, Border_Color border_color = {}); // :TextureCompression
Error_Code create_texture_from_memory(Texture *texture, u8 *buffer, s32 w, s32 h, u8 channels, Texture_Hints hints, Border_Color border_color = {});
void destroy_texture(Texture *texture);
void bind_texture(Texture *texture, s64 index_in_shader);



/* ------------------------------------------ Shader Constant Buffer ------------------------------------------ */

void create_shader_constant_buffer(Shader_Constant_Buffer *buffer, s64 size_in_bytes, void *initial_data = null);
void destroy_shader_constant_buffer(Shader_Constant_Buffer *buffer);
void update_shader_constant_buffer(Shader_Constant_Buffer *buffer, void *data);
void bind_shader_constant_buffer(Shader_Constant_Buffer *buffer, s64 index_in_shader, Shader_Type shader_types);



/* -------------------------------------------------- Shader -------------------------------------------------- */

Error_Code create_shader_from_file(Shader *shader, string file_path, Shader_Input_Specification *inputs, s64 input_count);
Error_Code create_shader_from_memory(Shader *shader, string _string, string name, Shader_Input_Specification *inputs, s64 input_count);
Error_Code create_shader_from_compiled_file(Shader *shader, string file_path, Shader_Input_Specification *inputs, s64 input_count); // :ShaderCompilation
Error_Code create_shader_from_compiled_memory(Shader *shader, string _string, string name, Shader_Input_Specification *inputs, s64 input_count); // :ShaderCompilation
void destroy_shader(Shader *shader);
void bind_shader(Shader *shader);

Compiled_Shader_Output compile_shader_from_file(string file_path);
void destroy_compiled_shader_output(Compiled_Shader_Output *output);



/* ----------------------------------------------- Frame Buffer ----------------------------------------------- */

void create_frame_buffer(Frame_Buffer *frame_buffer, u8 samples = 1);
void destroy_frame_buffer(Frame_Buffer *frame_buffer);
void create_frame_buffer_color_attachment(Frame_Buffer *frame_buffer, s32 w, s32 h, Frame_Buffer_Color_Format format = FRAME_BUFFER_COLOR_rgba8, b8 create_shader_view = false, Texture_Hints shader_view_hints = TEXTURE_HINT_None, Border_Color border_color = {});
void create_frame_buffer_depth_stencil_attachment(Frame_Buffer *frame_buffer, s32 w, s32 h, b8 create_shader_view = false, Texture_Hints shader_view_hints = TEXTURE_HINT_None, Border_Color border_color = {});
void resize_frame_buffer_color_attachment(Frame_Buffer *frame_buffer, s64 index, s32 w, s32 h);
void resize_frame_buffer_depth_stencil_attachment(Frame_Buffer *frame_buffer, s32 w, s32 h);
void resize_frame_buffer(Frame_Buffer *frame_buffer, s32 w, s32 h);
void bind_frame_buffer_color_attachment_to_shader(Frame_Buffer *frame_buffer, s64 attachment_index, s64 index_in_shader);
void bind_frame_buffer(Frame_Buffer *frame_buffer);
void clear_frame_buffer(Frame_Buffer *frame_buffer, f32 r, f32 g, f32 b, f32 a = 255, f32 depth = 1.f);
void blit_frame_buffer(Frame_Buffer *dst, Frame_Buffer *src);

Texture texture_wrapper_for_frame_buffer_color_attachment(Frame_Buffer *frame_buffer, s64 index);
Texture texture_wrapper_for_frame_buffer_depth_attachment(Frame_Buffer *frame_buffer);


/* ------------------------------------------------- Pipeline ------------------------------------------------- */

void create_pipeline_state(Pipeline_State *state); // User Level Input must be set before this!
void destroy_pipeline_state(Pipeline_State *state);
void bind_pipeline_state(Pipeline_State *state);
