#include "d3d11_layer.h"
#include "window.h"
#include "os_specific.h"
#include "memutils.h"

#define STB_IMAGE_IMPLEMENTATION
#include "Dependencies/stb_image.h"

#undef null // comdef.h has many parameters called 'null'... For fucks sake.

#include <d3d11.h>
#include <d3dcompiler.h>
#include <dxgi1_2.h>
#include <comdef.h>

#undef assert // Unfortunately comdef defines it's own assert (what a great idea that is microsoft...), so we should only be using foundation_assert in here. This ensures that.

#define null 0

#define D3D11_CALL(expr) d3d11_call_wrapper(expr, __FILE__ "," STRINGIFY(__LINE__) ": " STRINGIFY(expr))

struct Window_D3D11_State {
    IDXGISwapChain1 *swapchain;
    Frame_Buffer default_frame_buffer;
};

static_assert(sizeof(Window_D3D11_State) <= sizeof(Window::graphics_data), "Window_D3D11_State is bigger than expected.");



/* ------------------------------------------------- Globals ------------------------------------------------- */

s64 d3d_device_usage_count;
IDXGIDevice2  *dxgi_device       = null;
IDXGIAdapter  *dxgi_adapter      = null;
IDXGIFactory2 *dxgi_factory      = null;
ID3D11Device *d3d_device         = null;
ID3D11DeviceContext *d3d_context = null;
D3D_FEATURE_LEVEL d3d_feature_level;



/* ------------------------------------------------ Setup Code ------------------------------------------------ */

static
void d3d11_call_wrapper(HRESULT result, const char *assertion_text) {
    if(SUCCEEDED(result)) return;

    LPCTSTR wide_error_string = _com_error(result).ErrorMessage();
    size_t error_string_length;
    char error_string[256];
    wcstombs_s(&error_string_length, error_string, sizeof(error_string), wide_error_string, _TRUNCATE);
    foundation_do_assertion_fail(assertion_text, "[D3D11]: %.*s", (u32) error_string_length, error_string);
}

static
void create_d3d11_device() {
    ++d3d_device_usage_count;
    if(d3d_device != null) return;

    D3D_DRIVER_TYPE driver = D3D_DRIVER_TYPE_HARDWARE;
    UINT flags = D3D11_CREATE_DEVICE_SINGLETHREADED | D3D11_CREATE_DEVICE_DEBUG;
    UINT sdk = D3D11_SDK_VERSION;
    
    D3D11_CALL(D3D11CreateDevice(null, driver, null, flags, null, 0, sdk, &d3d_device, &d3d_feature_level, &d3d_context));

    D3D11_CALL(d3d_device->QueryInterface(__uuidof(IDXGIDevice2), (void **) &dxgi_device));
    D3D11_CALL(dxgi_device->GetParent(__uuidof(IDXGIAdapter), (void **) &dxgi_adapter));
    D3D11_CALL(dxgi_adapter->GetParent(__uuidof(IDXGIFactory2), (void **) &dxgi_factory));
}

static
void destroy_d3d11() {
    --d3d_device_usage_count;
    if(d3d_device_usage_count > 0) return;
    
    dxgi_factory->Release();
    dxgi_adapter->Release();
    dxgi_device->Release();
    d3d_context->Release();
    d3d_device->Release();
    d3d_context = null;
    d3d_device  = null;
}

enum D3D11_Data_Type {
    D3D11_UByte,
    D3D11_Float32,
};

static
DXGI_FORMAT d3d11_format(u8 channels, D3D11_Data_Type data_type) {
    DXGI_FORMAT format;

    switch(data_type) {
    case D3D11_UByte: {
        switch(channels) {
        case 1: format = DXGI_FORMAT_R8_UNORM; break;
        case 2: format = DXGI_FORMAT_R8G8_UNORM; break;
        case 4: format = DXGI_FORMAT_R8G8B8A8_UNORM; break;
        default: foundation_error("Invalid channel count for a D3D11 format."); break;
        }
    } break;

    case D3D11_Float32: {
        switch(channels) {
        case 1: format = DXGI_FORMAT_R32_FLOAT; break;
        case 2: format = DXGI_FORMAT_R32G32_FLOAT; break;
        case 3: format = DXGI_FORMAT_R32G32B32_FLOAT; break;
        case 4: format = DXGI_FORMAT_R32G32B32A32_FLOAT; break;
        default: foundation_error("Invalid channel count for a D3D11 format."); break;
        }
    } break;

    default:
        foundation_error("Invalid data type for a D3D11 format."); break;
        break;
    }

    return format;
}


void create_d3d11_context(Window *window) {
    create_d3d11_device();

    Window_D3D11_State *d3d11 = (Window_D3D11_State *) window->graphics_data;
    
    DXGI_FORMAT color_format = d3d11_format(4, D3D11_UByte);

    DXGI_SWAP_CHAIN_DESC1 swapchain_description{};
    swapchain_description.Width              = window->w;
    swapchain_description.Height             = window->h;
    swapchain_description.Format             = color_format;
    swapchain_description.Stereo             = FALSE;
    swapchain_description.SampleDesc.Count   = 1;
    swapchain_description.SampleDesc.Quality = 0;
    swapchain_description.BufferUsage        = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    swapchain_description.BufferCount        = 2;
    swapchain_description.Scaling            = DXGI_SCALING_STRETCH;
    swapchain_description.SwapEffect         = DXGI_SWAP_EFFECT_FLIP_SEQUENTIAL;
    swapchain_description.AlphaMode          = DXGI_ALPHA_MODE_UNSPECIFIED;
    swapchain_description.Flags              = 0;

    D3D11_CALL(dxgi_factory->CreateSwapChainForHwnd(d3d_device, (HWND) window_extract_hwnd(window), &swapchain_description, null, null, &d3d11->swapchain));

    d3d11->default_frame_buffer.samples     = 1;
    d3d11->default_frame_buffer.color_count = 1;
    d3d11->default_frame_buffer.colors[0].w = window->w;
    d3d11->default_frame_buffer.colors[0].h = window->h;
    d3d11->default_frame_buffer.colors[0].format = color_format;

    d3d11->default_frame_buffer.has_depth = false;

    D3D11_CALL(d3d11->swapchain->GetBuffer(0, __uuidof(ID3D11Texture2D), (void **) &d3d11->default_frame_buffer.colors[0].texture));
    D3D11_CALL(d3d_device->CreateRenderTargetView(d3d11->default_frame_buffer.colors[0].texture, 0, &d3d11->default_frame_buffer.colors[0].view));
}

void destroy_d3d11_context(Window *window) {
    Window_D3D11_State *d3d11 = (Window_D3D11_State *) window->graphics_data;
    destroy_frame_buffer(&d3d11->default_frame_buffer);
    d3d11->swapchain->Release();    
    destroy_d3d11();
}

void swap_d3d11_buffers(Window *window) {
    Window_D3D11_State *d3d11 = (Window_D3D11_State *) window->graphics_data;
    d3d11->swapchain->Present(1, 0);
}

Frame_Buffer *get_default_frame_buffer(Window *window) {
    Window_D3D11_State *d3d11 = (Window_D3D11_State *) window->graphics_data;    
    return &d3d11->default_frame_buffer;
}



/* ------------------------------------------------ Vertex Buffer ------------------------------------------------ */

void create_vertex_buffer(Vertex_Buffer *buffer, f32 *data, u64 float_count, u8 dimensions, Vertex_Buffer_Topology topology, b8 allow_updates) {
    foundation_assert(float_count % dimensions == 0, "Expected byte_count to be a multiple of dimensions.");

    buffer->vertex_count = float_count / dimensions;
    buffer->dimensions   = dimensions;
    buffer->topology     = topology;
    buffer->capacity     = float_count * sizeof(f32);

    D3D11_BUFFER_DESC buffer_description{};
    buffer_description.ByteWidth      = (UINT) (float_count * sizeof(f32));
    buffer_description.Usage          = allow_updates ? D3D11_USAGE_DYNAMIC : D3D11_USAGE_DEFAULT;
    buffer_description.BindFlags      = D3D11_BIND_VERTEX_BUFFER;
    buffer_description.CPUAccessFlags = allow_updates ? D3D11_CPU_ACCESS_WRITE : 0;

    D3D11_SUBRESOURCE_DATA subresource{};
    subresource.pSysMem = data;

    D3D11_CALL(d3d_device->CreateBuffer(&buffer_description, data != null ? &subresource : null, &buffer->handle));
}

void allocate_vertex_buffer(Vertex_Buffer *buffer, u64 float_count, u8 dimensions, Vertex_Buffer_Topology topology) {
    create_vertex_buffer(buffer, null, float_count, dimensions, topology, true);
}

void destroy_vertex_buffer(Vertex_Buffer *buffer) {
    buffer->handle->Release();
    buffer->handle       = null;
    buffer->vertex_count = 0;
    buffer->topology     = VERTEX_BUFFER_Undefined;
}

void update_vertex_buffer(Vertex_Buffer *buffer, f32 *data, u64 float_count) {
    foundation_assert(float_count % buffer->dimensions == 0, "Expected byte_count to be a multiple of dimensions.");
    foundation_assert(float_count * sizeof(f32) <= buffer->capacity, "A Vertex Buffer cannot be dynamically grown.");

    D3D11_MAPPED_SUBRESOURCE subresource;
    D3D11_CALL(d3d_context->Map(buffer->handle, 0, D3D11_MAP_WRITE_DISCARD, 0, &subresource)); // This might fail if we haven't supplied CPU Write Access to this buffer!
    memcpy(subresource.pData, data, float_count * sizeof(f32));
    d3d_context->Unmap(buffer->handle, 0);

    buffer->vertex_count = float_count / buffer->dimensions;
}

void bind_vertex_buffer(Vertex_Buffer *buffer) {
    UINT stride = buffer->dimensions * sizeof(f32), offset = 0;
    d3d_context->IASetPrimitiveTopology((D3D11_PRIMITIVE_TOPOLOGY) buffer->topology);
    d3d_context->IASetVertexBuffers(0, 1, &buffer->handle, &stride, &offset);
}

void draw_vertex_buffer(Vertex_Buffer *buffer) {
    d3d_context->Draw((UINT) buffer->vertex_count, 0);
}



/* --------------------------------------------- Vertex Buffer Array --------------------------------------------- */

void create_vertex_buffer_array(Vertex_Buffer_Array *array, Vertex_Buffer_Topology topology) {
    array->topology = topology;
    array->count = 0;
}

void destroy_vertex_buffer_array(Vertex_Buffer_Array *array) {
    for(s64 i = 0; i < array->count; ++i) {
        destroy_vertex_buffer(&array->buffers[i]);
    }
}

void add_vertex_data(Vertex_Buffer_Array *array, f32 *data, u64 float_count, u8 dimensions, b8 allow_updates) {
    foundation_assert(array->count < ARRAY_COUNT(array->buffers), "Vertex_Buffer_Array ran out of buffers.");
    create_vertex_buffer(&array->buffers[array->count], data, float_count, dimensions, array->topology, allow_updates);
    ++array->count;
}

void allocate_vertex_data(Vertex_Buffer_Array *array, u64 float_count, u8 dimensions) {
    add_vertex_data(array, null, float_count, dimensions, true);
}

void update_vertex_data(Vertex_Buffer_Array *array, s64 index, f32 *data, u64 float_count) {
    foundation_assert(index >= 0 && index < array->count);
    update_vertex_buffer(&array->buffers[index], data, float_count);
}

void bind_vertex_buffer_array(Vertex_Buffer_Array *array) {
    foundation_assert(array->count > 0);
    UINT strides[ARRAY_COUNT(array->buffers)], offsets[ARRAY_COUNT(array->buffers)];
    ID3D11Buffer *handles[ARRAY_COUNT(array->buffers)];

    for(s64 i = 0; i < array->count; ++i) {
        strides[i] = array->buffers[i].dimensions * sizeof(f32);
        offsets[i] = 0;
        handles[i] = array->buffers[i].handle;
    }

    d3d_context->IASetPrimitiveTopology((D3D11_PRIMITIVE_TOPOLOGY) array->topology);
    d3d_context->IASetVertexBuffers(0, (UINT) array->count, handles, strides, offsets);
}

void draw_vertex_buffer_array(Vertex_Buffer_Array *array) {
    foundation_assert(array->count > 0);
    d3d_context->Draw((UINT) array->buffers[0].vertex_count, 0);
}



/* --------------------------------------------------- Texture --------------------------------------------------- */

void create_texture_from_file(Texture *texture, string file_path) {
    char *cstring = to_cstring(Default_Allocator, file_path);
    defer { free_cstring(Default_Allocator, cstring); };

    int w, h, channels;

    // D3D11 (or rather the hardware but yeah) doesn't support 3 channel textures, so we unfortunately need to
    // convert all textures into rgba... (We don't have a way of saying only do this conversion from 3 to 4, but
    // leave 1 or 2 channels as they are...)
    u8 *buffer = stbi_load(cstring, &w, &h, &channels, 4);
    if(!buffer) {
        foundation_error("Failed to load texture '%.*s' from disk: %s.", stbi_failure_reason());
        return;
    }
    
    create_texture_from_memory(texture, buffer, w, h, channels);

    stbi_image_free(buffer);
}

void create_texture_from_memory(Texture *texture, u8 *bitmap, s32 w, s32 h, u8 channels) {
    texture->w = w;
    texture->h = h;
    texture->channels = channels;

    D3D11_TEXTURE2D_DESC texture_description{};
    texture_description.Width              = texture->w;
    texture_description.Height             = texture->h;
    texture_description.MipLevels          = 1;
    texture_description.ArraySize          = 1;
    texture_description.Format             = d3d11_format(texture->channels, D3D11_UByte);
    texture_description.SampleDesc.Count   = 1;
    texture_description.SampleDesc.Quality = 0;
    texture_description.Usage              = D3D11_USAGE_DEFAULT;
    texture_description.BindFlags          = D3D11_BIND_SHADER_RESOURCE;
    texture_description.CPUAccessFlags     = 0;
    texture_description.MiscFlags          = 0;
    
    D3D11_SUBRESOURCE_DATA subresource{};
    subresource.pSysMem          = bitmap;
    subresource.SysMemPitch      = texture->w * texture->channels;
    subresource.SysMemSlicePitch = 0;

    D3D11_SAMPLER_DESC sampler_description{};
    sampler_description.Filter         = D3D11_FILTER_MIN_MAG_MIP_LINEAR;
    sampler_description.AddressU       = D3D11_TEXTURE_ADDRESS_CLAMP;
    sampler_description.AddressV       = D3D11_TEXTURE_ADDRESS_CLAMP;
    sampler_description.AddressW       = D3D11_TEXTURE_ADDRESS_CLAMP;
    sampler_description.MipLODBias     = 0.f;
    sampler_description.MaxAnisotropy  = 1;
    sampler_description.ComparisonFunc = D3D11_COMPARISON_ALWAYS;
    sampler_description.MinLOD         = 0.f;
    sampler_description.MaxLOD         = 0.f;
    
    D3D11_CALL(d3d_device->CreateTexture2D(&texture_description, &subresource, &texture->handle));
    D3D11_CALL(d3d_device->CreateShaderResourceView(texture->handle, null, &texture->view));
    D3D11_CALL(d3d_device->CreateSamplerState(&sampler_description, &texture->sampler));
}

void destroy_texture(Texture *texture) {
    texture->sampler->Release();
    texture->view->Release();
    texture->handle->Release();
    texture->sampler  = null;
    texture->view     = null;
    texture->handle   = null;
    texture->w        = 0;
    texture->h        = 0;
    texture->channels = 0;
}

void bind_texture(Texture *texture, s64 index_in_shader) {
    d3d_context->PSSetSamplers((UINT) index_in_shader, 1, &texture->sampler);
    d3d_context->PSSetShaderResources((UINT) index_in_shader, 1, &texture->view);
}



/* ------------------------------------------ Shader Constant Buffer ------------------------------------------ */

void create_shader_constant_buffer(Shader_Constant_Buffer *buffer, s64 index_in_shader, s64 size_in_bytes, void *initial_data) {
    D3D11_BUFFER_DESC description{};
    description.Usage          = D3D11_USAGE_DEFAULT;
    description.ByteWidth      = align_to(size_in_bytes, 16, UINT);
    description.BindFlags      = D3D11_BIND_CONSTANT_BUFFER;
    description.CPUAccessFlags = 0;
    description.MiscFlags      = 0;

    D3D11_SUBRESOURCE_DATA subresource{};
    subresource.pSysMem = initial_data;

    D3D11_CALL(d3d_device->CreateBuffer(&description, (initial_data != null) ? &subresource : null, &buffer->handle));

    buffer->index_in_shader = index_in_shader;
}

void destroy_shader_constant_buffer(Shader_Constant_Buffer *buffer) {
    buffer->handle->Release();
    buffer->handle = null;
    buffer->index_in_shader = 0;
}

void update_shader_constant_buffer(Shader_Constant_Buffer *buffer, void *data) {
    // @@Speed: This should probably go with Map / Unmap as well.
    d3d_context->UpdateSubresource(buffer->handle, 0, null, data, 0, 0);
}

void bind_shader_constant_buffer(Shader_Constant_Buffer *buffer, Shader_Type shader_types) {
    if(shader_types & SHADER_Vertex) d3d_context->VSSetConstantBuffers((UINT) buffer->index_in_shader, 1, &buffer->handle);
    if(shader_types & SHADER_Pixel)  d3d_context->PSSetConstantBuffers((UINT) buffer->index_in_shader, 1, &buffer->handle);
}



/* -------------------------------------------------- Shader -------------------------------------------------- */

void create_shader_from_file(Shader *shader, string file_path, Shader_Input_Specification *inputs, s64 input_count) {
    foundation_assert(input_count < D3D11_MAX_SHADER_INPUTS);

    ID3DBlob *error_blob = null;

    string file_content = os_read_file(Default_Allocator, file_path);
    if(!file_content.count) {
        foundation_error("Failed to load shader '%.*s' from disk: The file does not exist.", (u32) file_path.count, file_path.data);
        return;
    }

    char *file_path_cstring = to_cstring(Default_Allocator, file_path);
    D3DCompile(file_content.data, file_content.count, file_path_cstring, null, D3D_COMPILE_STANDARD_FILE_INCLUDE, "vs_main", "vs_5_0", D3DCOMPILE_ENABLE_STRICTNESS | D3DCOMPILE_DEBUG, 0, &shader->vertex_blob, &error_blob);
    D3DCompile(file_content.data, file_content.count, file_path_cstring, null, D3D_COMPILE_STANDARD_FILE_INCLUDE, "ps_main", "ps_5_0", D3DCOMPILE_ENABLE_STRICTNESS | D3DCOMPILE_DEBUG, 0, &shader->pixel_blob, &error_blob);

    if(shader->vertex_blob && shader->pixel_blob) {
        D3D11_CALL(d3d_device->CreateVertexShader(shader->vertex_blob->GetBufferPointer(), shader->vertex_blob->GetBufferSize(), null, &shader->vertex_shader));
        D3D11_CALL(d3d_device->CreatePixelShader(shader->pixel_blob->GetBufferPointer(), shader->pixel_blob->GetBufferSize(), null, &shader->pixel_shader));

        if(shader->vertex_shader && shader->pixel_shader) {
            D3D11_INPUT_ELEMENT_DESC input_element_description[D3D11_MAX_SHADER_INPUTS];

            for(s64 i = 0; i < input_count; ++i) {                
                input_element_description[i] = {
                    inputs[i].name, 0, d3d11_format(inputs[i].dimensions, D3D11_Float32), inputs[i].vertex_buffer_index, 0, D3D11_INPUT_PER_VERTEX_DATA, 0
                };
            }

            D3D11_CALL(d3d_device->CreateInputLayout(input_element_description, (UINT) input_count, shader->vertex_blob->GetBufferPointer(), shader->vertex_blob->GetBufferSize(), &shader->input_layout));
        } else {
            foundation_error("Failed to create shader '%.*s'.", (u32) file_path.count, file_path.data);
            destroy_shader(shader);
        }
    } else {
        foundation_assert(error_blob != null);
        foundation_error("Failed to compile shader '%.*s': '%s'.", (u32) file_path.count, file_path.data, error_blob->GetBufferPointer());
        destroy_shader(shader);
    }
    
    free_cstring(Default_Allocator, file_path_cstring);
    os_free_file_content(Default_Allocator, &file_content);
    if(error_blob) error_blob->Release();
}

void destroy_shader(Shader *shader) {
    if(shader->vertex_blob)   shader->vertex_blob->Release();
    if(shader->pixel_blob)    shader->pixel_blob->Release();
    if(shader->vertex_shader) shader->vertex_shader->Release();
    if(shader->pixel_shader)  shader->pixel_shader->Release();
    if(shader->input_layout)  shader->input_layout->Release();
}

void bind_shader(Shader *shader) {
    d3d_context->VSSetShader(shader->vertex_shader, null, 0);
    d3d_context->PSSetShader(shader->pixel_shader, null, 0);
    d3d_context->IASetInputLayout(shader->input_layout);
}



/* ------------------------------------------------- Frame Buffer ------------------------------------------------- */

void create_frame_buffer(Frame_Buffer *frame_buffer, u8 samples) {
    frame_buffer->samples     = samples;
    frame_buffer->color_count = 0;
    frame_buffer->has_depth   = 0;
}

void destroy_frame_buffer(Frame_Buffer *frame_buffer) {
    for(s64 i = 0; i < frame_buffer->color_count; ++i) {
        frame_buffer->colors[i].view->Release();
        frame_buffer->colors[i].texture->Release();
    }

    frame_buffer->color_count = 0;

    if(frame_buffer->has_depth) {
        frame_buffer->depth_stencil_texture->Release();
        frame_buffer->depth_stencil_view->Release();
        frame_buffer->depth_stencil_state->Release();
    }
    
    frame_buffer->has_depth = false;
}

// @Incomplete: Add HDR frame buffers

void create_frame_buffer_color_attachment(Frame_Buffer *frame_buffer, s32 w, s32 h, b8 hdr) {
    foundation_assert(frame_buffer->color_count < ARRAY_COUNT(frame_buffer->colors));

    Frame_Buffer_Attachment *attachment = &frame_buffer->colors[frame_buffer->color_count];
    attachment->w = w;
    attachment->h = h;
    attachment->format = d3d11_format(4, hdr ? D3D11_Float32 : D3D11_UByte);
    
    D3D11_TEXTURE2D_DESC texture_description{};
    texture_description.Width              = w;
    texture_description.Height             = h;
    texture_description.MipLevels          = 1;
    texture_description.ArraySize          = 1;
    texture_description.Format             = (DXGI_FORMAT) attachment->format;
    texture_description.SampleDesc.Count   = frame_buffer->samples;
    texture_description.SampleDesc.Quality = 0;
    texture_description.Usage              = D3D11_USAGE_DEFAULT;
    texture_description.BindFlags          = D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE;
    texture_description.CPUAccessFlags     = 0;
    texture_description.MiscFlags          = 0;

    D3D11_RENDER_TARGET_VIEW_DESC view_description{};
    view_description.Format             = (DXGI_FORMAT) attachment->format;
    view_description.ViewDimension      = frame_buffer->samples > 1 ? D3D11_RTV_DIMENSION_TEXTURE2DMS : D3D11_RTV_DIMENSION_TEXTURE2D;
    view_description.Texture2D.MipSlice = 0;

    D3D11_CALL(d3d_device->CreateTexture2D(&texture_description, null, &attachment->texture));
    D3D11_CALL(d3d_device->CreateRenderTargetView(attachment->texture, &view_description, &attachment->view));

    ++frame_buffer->color_count;
}

void create_frame_buffer_depth_stencil_attachment(Frame_Buffer *frame_buffer, s32 w, s32 h) {
    foundation_assert(frame_buffer->has_depth == false);

    frame_buffer->depth_format = DXGI_FORMAT_D24_UNORM_S8_UINT;

    D3D11_DEPTH_STENCILOP_DESC operations{};
    operations.StencilFailOp      = D3D11_STENCIL_OP_KEEP;
    operations.StencilDepthFailOp = D3D11_STENCIL_OP_KEEP;
    operations.StencilPassOp      = D3D11_STENCIL_OP_REPLACE;
    operations.StencilFunc        = D3D11_COMPARISON_ALWAYS;
    
    D3D11_DEPTH_STENCIL_DESC state_description{};
    state_description.DepthEnable      = TRUE;
    state_description.DepthWriteMask   = D3D11_DEPTH_WRITE_MASK_ALL;
    state_description.DepthFunc        = D3D11_COMPARISON_LESS;
    state_description.StencilEnable    = FALSE;
    state_description.StencilReadMask  = 0xff;
    state_description.StencilWriteMask = 0xff;
    state_description.FrontFace        = operations;
    state_description.BackFace         = operations;

    D3D11_TEXTURE2D_DESC texture_description{};
    texture_description.Width              = w;
    texture_description.Height             = h;
    texture_description.MipLevels          = 1;
    texture_description.ArraySize          = 1;
    texture_description.Format             = (DXGI_FORMAT) frame_buffer->depth_format;
    texture_description.SampleDesc.Count   = frame_buffer->samples;
    texture_description.SampleDesc.Quality = 0;
    texture_description.Usage              = D3D11_USAGE_DEFAULT;
    texture_description.BindFlags          = D3D11_BIND_DEPTH_STENCIL;
    texture_description.CPUAccessFlags     = 0;
    texture_description.MiscFlags          = 0;

    D3D11_DEPTH_STENCIL_VIEW_DESC view_description{};
    view_description.Format             = (DXGI_FORMAT) frame_buffer->depth_format;
    view_description.ViewDimension      = frame_buffer->samples > 1 ? D3D11_DSV_DIMENSION_TEXTURE2DMS : D3D11_DSV_DIMENSION_TEXTURE2D;
    view_description.Texture2D.MipSlice = 0;
    
    D3D11_CALL(d3d_device->CreateDepthStencilState(&state_description, &frame_buffer->depth_stencil_state));
    D3D11_CALL(d3d_device->CreateTexture2D(&texture_description, null, &frame_buffer->depth_stencil_texture));
    D3D11_CALL(d3d_device->CreateDepthStencilView(frame_buffer->depth_stencil_texture, &view_description, &frame_buffer->depth_stencil_view));

    frame_buffer->has_depth = true;
}

void bind_frame_buffer(Frame_Buffer *frame_buffer) {
    D3D11_VIEWPORT viewports[ARRAY_COUNT(frame_buffer->colors)];
    for(s64 i = 0; i < frame_buffer->color_count; ++i) {
        viewports[i] = { 0.f, 0.f, (f32) frame_buffer->colors[i].w, (f32) frame_buffer->colors[i].h, 0.f, 1.f };
    }
    d3d_context->RSSetViewports((UINT) frame_buffer->color_count, viewports);

    ID3D11RenderTargetView *views[ARRAY_COUNT(frame_buffer->colors)];
    for(s64 i = 0; i < frame_buffer->color_count; ++i) {
        views[i] = frame_buffer->colors[i].view;
    }
    d3d_context->OMSetRenderTargets((UINT) frame_buffer->color_count, views, (frame_buffer->has_depth) ? frame_buffer->depth_stencil_view : null);
}

void clear_frame_buffer(Frame_Buffer *frame_buffer, f32 r, f32 g, f32 b) {
    f32 color_array[4];
    color_array[0] = r;
    color_array[1] = g;
    color_array[2] = b;
    color_array[3] = 1.f;

    for(s64 i = 0; i < frame_buffer->color_count; ++i) {
        d3d_context->ClearRenderTargetView(frame_buffer->colors[i].view, color_array);
    }

    if(frame_buffer->has_depth) d3d_context->ClearDepthStencilView(frame_buffer->depth_stencil_view, D3D11_CLEAR_DEPTH  | D3D11_CLEAR_STENCIL, 1.f, 0);
}

void blit_frame_buffer(Frame_Buffer *dst, Frame_Buffer *src) {
    foundation_assert(dst->color_count == src->color_count);
    foundation_assert(dst->samples == 1 || src->samples == dst->samples); // We can only either copy the samples into the destination, or resolve them into a single one.
    
    if(src->samples == dst->samples) {
        for(s64 i = 0; i < dst->color_count; ++i) {
            foundation_assert(dst->colors[i].w == src->colors[i].w && dst->colors[i].h == src->colors[i].h);
            foundation_assert(dst->colors[i].format == src->colors[i].format);
            d3d_context->CopyResource(dst->colors[i].texture, src->colors[i].texture);
        }

        if(dst->has_depth && src->has_depth) {
            foundation_assert(dst->depth_format == src->depth_format);
            d3d_context->CopyResource(dst->depth_stencil_texture, src->depth_stencil_texture);
        }
    } else {
        for(s64 i = 0; i < dst->color_count; ++i) {
            foundation_assert(dst->colors[i].w == src->colors[i].w && dst->colors[i].h == src->colors[i].h);
            foundation_assert(dst->colors[i].format == src->colors[i].format);
            d3d_context->ResolveSubresource(dst->colors[i].texture, 0, src->colors[i].texture, 0, (DXGI_FORMAT) dst->colors[i].format);
        }

        if(dst->has_depth && src->has_depth) {
            foundation_assert(dst->depth_format == src->depth_format);
            d3d_context->ResolveSubresource(dst->depth_stencil_texture, 0, src->depth_stencil_texture, 0, (DXGI_FORMAT) dst->depth_format);
        }
    }
}



/* ------------------------------------------------ Pipeline State ------------------------------------------------ */

void create_pipeline_state(Pipeline_State *state) {
    D3D11_RASTERIZER_DESC rasterizer{};
    rasterizer.FillMode = D3D11_FILL_SOLID;
    rasterizer.CullMode = (state->enable_culling) ? D3D11_CULL_BACK : D3D11_CULL_NONE;
    rasterizer.FrontCounterClockwise = FALSE;
    rasterizer.DepthBias             = 0;
    rasterizer.DepthBiasClamp        = 0;
    rasterizer.SlopeScaledDepthBias  = 0;
    rasterizer.DepthClipEnable       = state->enable_depth_test;
    rasterizer.ScissorEnable         = state->enable_scissors;
    rasterizer.MultisampleEnable     = state->enable_multisample;
    rasterizer.AntialiasedLineEnable = FALSE;
    D3D11_CALL(d3d_device->CreateRasterizerState(&rasterizer, &state->handle));
}

void destroy_pipeline_state(Pipeline_State *state) {
    state->handle->Release();
    state->handle = null;
}

void bind_pipeline_state(Pipeline_State *state) {
    d3d_context->RSSetState(state->handle);
}
