#pragma once

#include "foundation.h"
#include "strings.h"

//
// Forward declarations to avoid having to include the D3D11 headers here.
//

struct ID3D10Blob;
typedef ID3D10Blob ID3DBlob;

struct ID3D11Buffer;
struct ID3D11ShaderResourceView;
struct ID3D11SamplerState;
struct ID3D11Texture2D;
struct ID3D11VertexShader;
struct ID3D11PixelShader;
struct ID3D11InputLayout;
struct ID3D11RenderTargetView;
struct ID3D11RasterizerState;

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
    ID3D11Buffer *handle;
};

struct Vertex_Buffer_Array {
    Vertex_Buffer_Topology topology;
    Vertex_Buffer buffers[D3D11_MAX_VERTEX_BUFFERS];
    s64 count;
};

struct Texture {
    s32 w, h;
    ID3D11ShaderResourceView *view;
    ID3D11SamplerState *sampler_state;
    ID3D11Texture2D *handle;
};

struct Shader_Input_Specification {
    const char *name;
    u8 dimensions;
    u8 vertex_buffer_index;
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
    ID3D11RenderTargetView *view;
    ID3D11Texture2D *texture;
};

struct Frame_Buffer {
    Frame_Buffer_Attachment colors[D3D11_MAX_FRAMEBUFFER_COLOR_ATTACHMENTS];
    s64 color_count;
};

struct Pipeline_State {
    // --- User Level Input
    b8 enable_culling;
    b8 enable_depth_test;
    b8 enable_scissors;
    b8 enable_multisample;
    
    // --- Internal Handle
    ID3D11RasterizerState *handle;
};

void create_d3d11_context(Window *window);
void destroy_d3d11_context(Window *window);
void clear_d3d11_buffer(Window *window, u8 r, u8 g, u8 b);
void swap_d3d11_buffers(Window *window);

void create_vertex_buffer(Vertex_Buffer *buffer, f32 *data, u64 float_count, u8 dimensions, Vertex_Buffer_Topology topology);
void destroy_vertex_buffer(Vertex_Buffer *buffer);
void bind_vertex_buffer(Vertex_Buffer *buffer);
void draw_vertex_buffer(Vertex_Buffer *buffer);

void create_vertex_buffer_array(Vertex_Buffer_Array *array, Vertex_Buffer_Topology topology);
void destroy_vertex_buffer_array(Vertex_Buffer_Array *array);
void add_vertex_data(Vertex_Buffer_Array *array, f32 *data, u64 float_count, u8 dimensions);
void bind_vertex_buffer_array(Vertex_Buffer_Array *array);
void draw_vertex_buffer_array(Vertex_Buffer_Array *array);

void create_shader_from_file(Shader *shader, string file_path, Shader_Input_Specification *inputs, s64 input_count);
void destroy_shader(Shader *shader);
void bind_shader(Shader *shader);

void create_pipeline_state(Pipeline_State *state); // User Level Input must be set before this!
void destroy_pipeline_state(Pipeline_State *state);
void bind_pipeline_state(Pipeline_State *state);