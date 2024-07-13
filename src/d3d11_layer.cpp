#include "d3d11_layer.h"
#include "window.h"
#include "os_specific.h"
#include "memutils.h"

#include "Dependencies/stb_image.h"

#undef null // comdef.h has many parameters called 'null'... For fucks sake.

#include <d3d11.h>
#include <d3dcompiler.h>
#include <dxgi1_2.h>
#include <comdef.h>

#undef assert // Unfortunately comdef defines it's own assert (what a great idea that is microsoft...), so we should only be using foundation_assert in here. This ensures that.

#define null 0

#define D3D11_BACKBUFFER_COLOR_FORMAT DXGI_FORMAT_R8G8B8A8_UNORM
#define D3D11_CALL(expr) d3d11_call_wrapper(expr, __FILE__ "," STRINGIFY(__LINE__) ": " STRINGIFY(expr))

struct Window_D3D11_State {
    IDXGISwapChain1 *swapchain;
    Frame_Buffer *default_frame_buffer; // This thing is huge, so we sorta need to allocate it dynamically, or else the Window's graphics buffer gets really really big.
};

static_assert(sizeof(Window_D3D11_State) <= sizeof(Window::graphics_data), "Window_D3D11_State is bigger than expected.");



/* ------------------------------------------------- Globals ------------------------------------------------- */

s64 d3d_device_usage_count = 0;
IDXGIDevice2  *dxgi_device       = null;
IDXGIAdapter  *dxgi_adapter      = null;
IDXGIFactory2 *dxgi_factory      = null;
ID3D11Device *d3d_device         = null;
ID3D11DeviceContext *d3d_context = null;
D3D_FEATURE_LEVEL d3d_feature_level;



/* --------------------------------------------- Internal Helpers --------------------------------------------- */

enum D3D11_Data_Type {
    D3D11_UByte,
    D3D11_Float32,
};

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
DXGI_FORMAT d3d11_format(u8 channels, D3D11_Data_Type data_type, Texture_Hints hints = TEXTURE_HINT_None) {
    b8 srgb = (hints & TEXTURE_Is_In_Srgb) != 0;
    
    if(hints & TEXTURE_COMPRESS_BC7) {
        if(data_type != D3D11_UByte) return (DXGI_FORMAT) -2; // Invalid data type
        return srgb ? DXGI_FORMAT_BC7_UNORM_SRGB : DXGI_FORMAT_BC7_UNORM;
    }
    
    DXGI_FORMAT format;
    
    switch(data_type) {
        case D3D11_UByte: {
            switch(channels) {
                case 1:  format = DXGI_FORMAT_R8_UNORM; break;
                case 2:  format = DXGI_FORMAT_R8G8_UNORM; break;
                case 4:  format = srgb ? DXGI_FORMAT_R8G8B8A8_UNORM_SRGB : DXGI_FORMAT_R8G8B8A8_UNORM; break;
                default: format = (DXGI_FORMAT) -1; break; // Invalid channel count
            }
        } break;
        
        case D3D11_Float32: {
            switch(channels) {
                case 1: format = DXGI_FORMAT_R32_FLOAT; break;
                case 2: format = DXGI_FORMAT_R32G32_FLOAT; break;
                case 3: format = DXGI_FORMAT_R32G32B32_FLOAT; break;
                case 4: format = DXGI_FORMAT_R32G32B32A32_FLOAT; break;
                default: format = (DXGI_FORMAT) -1; break; // Invalid channel count
            }
        } break;
        
        default:
        format = (DXGI_FORMAT) -2; break; // Invalid data type
        break;
    }
    
    return format;
}

static
DXGI_FORMAT d3d11_format(Frame_Buffer_Color_Format input) {
    DXGI_FORMAT output;
    
    switch(input) {
        case FRAME_BUFFER_COLOR_rgba8:  output = DXGI_FORMAT_R8G8B8A8_UNORM;     break;
        case FRAME_BUFFER_COLOR_rgba16: output = DXGI_FORMAT_R16G16B16A16_FLOAT; break;
        case FRAME_BUFFER_COLOR_rgba32: output = DXGI_FORMAT_R32G32B32A32_FLOAT; break;
    }
    
    return output;
}

static
DXGI_FORMAT texture_format_for_depth_format(DXGI_FORMAT depth) {
    DXGI_FORMAT texture;
    
    switch(depth) {
        case DXGI_FORMAT_D24_UNORM_S8_UINT:
        texture = DXGI_FORMAT_R24G8_TYPELESS;
        break;
        
        default:
        foundation_error("Invalid depth format.");
        break;
    }
    
    return texture;
}

static
DXGI_FORMAT resource_format_for_depth_format(DXGI_FORMAT depth) {
    DXGI_FORMAT resource;
    
    switch(depth) {
        case DXGI_FORMAT_D24_UNORM_S8_UINT:
        resource = DXGI_FORMAT_R24_UNORM_X8_TYPELESS;
        break;
        
        default:
        foundation_error("Invalid depth format.");
        break;
    }
    
    return resource;
}

static
D3D11_FILTER d3d11_filter(Texture_Hints hints) {
    D3D11_FILTER output;
    
    Texture_Hints input = hints & TEXTURE_FILTER;
    
    switch(input) {
        case TEXTURE_FILTER_Nearest: output = D3D11_FILTER_MIN_MAG_MIP_POINT;  break;
        case TEXTURE_FILTER_Linear:  output = D3D11_FILTER_MIN_MAG_MIP_LINEAR; break;
        case TEXTURE_FILTER_Comparison_Nearest: output = D3D11_FILTER_COMPARISON_MIN_MAG_MIP_POINT;  break;
        case TEXTURE_FILTER_Comparison_Linear:  output = D3D11_FILTER_COMPARISON_MIN_MAG_MIP_LINEAR; break;
        default: output = D3D11_FILTER_MIN_MAG_MIP_POINT; break;
    }
    
    return output;
}

static
D3D11_TEXTURE_ADDRESS_MODE d3d11_texture_address_mode(Texture_Hints hints) {
    D3D11_TEXTURE_ADDRESS_MODE output;
    
    Texture_Hints input = hints & TEXTURE_WRAP;
    
    switch(input) {
        case TEXTURE_WRAP_Repeat: output = D3D11_TEXTURE_ADDRESS_WRAP;   break;
        case TEXTURE_WRAP_Edge:   output = D3D11_TEXTURE_ADDRESS_CLAMP;  break;
        case TEXTURE_WRAP_Border: output = D3D11_TEXTURE_ADDRESS_BORDER; break;
        default: output = D3D11_TEXTURE_ADDRESS_BORDER; break;
    }
    
    return output;
}

static
u8 get_channels_for_d3d11_format(DXGI_FORMAT format) {
    u8 channels = -1;
    
    switch(format) {
        case DXGI_FORMAT_R8_UNORM:
        case DXGI_FORMAT_R32_FLOAT:
        case DXGI_FORMAT_D24_UNORM_S8_UINT:
        channels = 1;
        break;
        
        case DXGI_FORMAT_R8G8_UNORM:
        case DXGI_FORMAT_R32G32_FLOAT:
        channels = 2;
        break;
        
        case DXGI_FORMAT_R8G8B8A8_UNORM:
        case DXGI_FORMAT_R32G32B32A32_FLOAT:
        channels = 4;
        break;
        
        default: foundation_error("Unknown format."); break;
    }
    
    return channels;
}



/* ---------------------------------------------- Context Setup ---------------------------------------------- */

static
void create_d3d11_device() {
    ++d3d_device_usage_count;
    if(d3d_device != null) return;
    
    D3D_DRIVER_TYPE driver = D3D_DRIVER_TYPE_HARDWARE;
    
#if FOUNDATION_DEVELOPER
    UINT flags = D3D11_CREATE_DEVICE_SINGLETHREADED | D3D11_CREATE_DEVICE_DEBUG;
#else
    UINT flags = D3D11_CREATE_DEVICE_SINGLETHREADED;
#endif
    
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

static
void restore_d3d11_fullscreen_state(Window *window) {
    set_d3d11_fullscreen(window, window->active, true);
}

void create_d3d11_context(Window *window, b8 fullscreen) {
    create_d3d11_device();

    Window_D3D11_State *d3d11 = (Window_D3D11_State *) window->graphics_data;
    
    DXGI_SWAP_CHAIN_DESC1 swapchain_description{};
    swapchain_description.Width              = window->w;
    swapchain_description.Height             = window->h;
    swapchain_description.Format             = D3D11_BACKBUFFER_COLOR_FORMAT;
    swapchain_description.Stereo             = FALSE;
    swapchain_description.SampleDesc.Count   = 1;
    swapchain_description.SampleDesc.Quality = 0;
    swapchain_description.BufferUsage        = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    swapchain_description.BufferCount        = 2;
    swapchain_description.Scaling            = DXGI_SCALING_STRETCH;
    swapchain_description.SwapEffect         = DXGI_SWAP_EFFECT_FLIP_SEQUENTIAL;
    swapchain_description.AlphaMode          = DXGI_ALPHA_MODE_UNSPECIFIED;
    swapchain_description.Flags              = DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH;
    
    D3D11_CALL(dxgi_factory->CreateSwapChainForHwnd(d3d_device, (HWND) window_extract_hwnd(window), &swapchain_description, null, null, &d3d11->swapchain));

    d3d11->default_frame_buffer = (Frame_Buffer *) Default_Allocator->allocate(sizeof(Frame_Buffer));
    d3d11->default_frame_buffer->samples          = 1;
    d3d11->default_frame_buffer->color_count      = 1;
    d3d11->default_frame_buffer->colors[0].w      = window->w;
    d3d11->default_frame_buffer->colors[0].h      = window->h;
    d3d11->default_frame_buffer->colors[0].format = D3D11_BACKBUFFER_COLOR_FORMAT;
    
    d3d11->default_frame_buffer->has_depth = false;
    
    D3D11_CALL(d3d11->swapchain->GetBuffer(0, __uuidof(ID3D11Texture2D), (void **) &d3d11->default_frame_buffer->colors[0].texture));
    D3D11_CALL(d3d_device->CreateRenderTargetView(d3d11->default_frame_buffer->colors[0].texture, 0, &d3d11->default_frame_buffer->colors[0].render_view));    
    
    if(fullscreen) set_d3d11_fullscreen(window, true);
}

void destroy_d3d11_context(Window *window) {
    Window_D3D11_State *d3d11 = (Window_D3D11_State *) window->graphics_data;
    d3d11->swapchain->SetFullscreenState(false, null); // For proper cleanup, because D3D11 might keep some interal things or something? We get a leak otherwise
    d3d11->default_frame_buffer->colors[0].texture->Release();
    d3d11->default_frame_buffer->colors[0].render_view->Release();
    d3d11->swapchain->Release();
    destroy_d3d11();
    Default_Allocator->deallocate(d3d11->default_frame_buffer);
    memset(d3d11, 0, sizeof(Window_D3D11_State));
}

void swap_d3d11_buffers(Window *window) {
    Window_D3D11_State *d3d11 = (Window_D3D11_State *) window->graphics_data;
    d3d11->swapchain->Present(1, 0);
}

void set_d3d11_fullscreen(Window *window, b8 fullscreen, b8 internal_restoration) {
    Window_D3D11_State *d3d11 = (Window_D3D11_State *) window->graphics_data;
    if(d3d11->swapchain) {
        D3D11_CALL(d3d11->swapchain->SetFullscreenState(fullscreen, null));
        resize_default_frame_buffer(window);

        // This is required so restore the fullscreen state when our window gets activated again.
        // If the user tabs out of this game, we obviously want to release our fullscreen mode so
        // that the user can actually use their machine.
        window->callback_on_activation = fullscreen || internal_restoration ? (Window_Callback) restore_d3d11_fullscreen_state : null;
    }
}

void clear_d3d11_state() {
    d3d_context->ClearState();
}

Frame_Buffer *get_default_frame_buffer(Window *window) {
    Window_D3D11_State *d3d11 = (Window_D3D11_State *) window->graphics_data;    
    return d3d11->default_frame_buffer;
}

void resize_default_frame_buffer(Window *window) {
    Window_D3D11_State *d3d11 = (Window_D3D11_State *) window->graphics_data;
    
    if(d3d11->swapchain) {
        // Release all outstanding references to the swap chain's buffer, because we cannot
        // resize the swap chain otherwise.
        d3d11->default_frame_buffer->colors[0].texture->Release();
        d3d11->default_frame_buffer->colors[0].render_view->Release();
    
        // Actually resize the buffer.
        D3D11_CALL(d3d11->swapchain->ResizeBuffers(0, window->w, window->h, DXGI_FORMAT_UNKNOWN, DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH));
    
        // Get the render target view back.
        D3D11_CALL(d3d11->swapchain->GetBuffer(0, __uuidof(ID3D11Texture2D), (void **) &d3d11->default_frame_buffer->colors[0].texture));
        D3D11_CALL(d3d_device->CreateRenderTargetView(d3d11->default_frame_buffer->colors[0].texture, 0, &d3d11->default_frame_buffer->colors[0].render_view));
    
        d3d11->default_frame_buffer->colors[0].w = window->w;
        d3d11->default_frame_buffer->colors[0].h = window->h;
    }
}



/* ---------------------------------------------- Vertex Buffer ----------------------------------------------- */

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



/* -------------------------------------------- Vertex Buffer Array ------------------------------------------- */

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



/* ------------------------------------------------- Texture ------------------------------------------------- */

Error_Code create_texture_from_file(Texture *texture, string file_path, Texture_Hints hints, Border_Color border_color) {
    char *cstring = to_cstring(Default_Allocator, file_path);
    defer { free_cstring(Default_Allocator, cstring); };
    
    int w, h, channels;
    
    // D3D11 (or rather the hardware but yeah) doesn't support 3 channel textures, so we unfortunately need to
    // convert all textures into rgba... (We don't have a way of saying only do this conversion from 3 to 4, but
    // leave 1 or 2 channels as they are...)
    u8 *buffer = stbi_load(cstring, &w, &h, &channels, 4);
    if(!buffer) {
        set_custom_error_message(stbi_failure_reason());
        return ERROR_Custom_Error_Message;
    }
    
    Error_Code error = create_texture_from_memory(texture, buffer, w, h, 4, hints, border_color); // 'channels' contains the original channels in the file, not the converted output which we forced to 4
    
    stbi_image_free(buffer);
    return error;
}

Error_Code create_texture_from_compressed_file(Texture *texture, string file_path, Texture_Hints hints, Border_Color border_color) {
    string file_content = os_read_file(Default_Allocator, file_path);
    if(!file_content.count) {
        return ERROR_File_Not_Found;
    }
    
    Error_Code error = create_texture_from_compressed_memory(texture, file_content, hints, border_color);
    os_free_file_content(Default_Allocator, &file_content);
    return error;
}

Error_Code create_texture_from_compressed_memory(Texture *texture, string file_content, Texture_Hints hints, Border_Color border_color) {
    s32 width    = *(s32 *) &file_content.data[0];
    s32 height   = *(s32 *) &file_content.data[4];
    s32 channels = *(s32 *) &file_content.data[8];
    u8 *data     =  (u8  *) &file_content.data[12];
    Error_Code error = create_texture_from_memory(texture, data, width, height, (u8) channels, hints | TEXTURE_COMPRESS_BC7, border_color);
    return error;
}

Error_Code create_texture_from_memory(Texture *texture, u8 *buffer, s32 w, s32 h, u8 channels, Texture_Hints hints, Border_Color border_color) {
    if(w <= 0 || h <= 0 || w >= 65535 || h >= 65535) return ERROR_D3D11_Invalid_Dimensions;
    
    DXGI_FORMAT format  = d3d11_format(channels, D3D11_UByte, hints);
    D3D11_FILTER filter = d3d11_filter(hints);
    D3D11_TEXTURE_ADDRESS_MODE texture_address_mode = d3d11_texture_address_mode(hints);
    
    if(format == -1) return ERROR_D3D11_Invalid_Channel_Count;
    if(format == -2) return ERROR_D3D11_Invalid_Data_Type;
    
    texture->w = w;
    texture->h = h;
    texture->channels = channels;
    
    D3D11_TEXTURE2D_DESC texture_description{};
    texture_description.Width              = texture->w;
    texture_description.Height             = texture->h;
    texture_description.MipLevels          = 1;
    texture_description.ArraySize          = 1;
    texture_description.Format             = format;
    texture_description.SampleDesc.Count   = 1;
    texture_description.SampleDesc.Quality = 0;
    texture_description.Usage              = D3D11_USAGE_DEFAULT;
    texture_description.BindFlags          = D3D11_BIND_SHADER_RESOURCE;
    texture_description.CPUAccessFlags     = 0;
    texture_description.MiscFlags          = 0;
    
    u32 pitch, slice_pitch;
    
    if(hints & TEXTURE_COMPRESS_BC7) {
        pitch = 16 * (texture->w / 4);
        slice_pitch = pitch * (texture->h / 4);
    } else {
        pitch = texture->w * texture->channels;
        slice_pitch = pitch * texture->h;
    }
    
    D3D11_SUBRESOURCE_DATA subresource{};
    subresource.pSysMem          = buffer;
    subresource.SysMemPitch      = pitch;
    subresource.SysMemSlicePitch = slice_pitch;
    
    D3D11_SAMPLER_DESC sampler_description{};
    sampler_description.Filter         = filter;
    sampler_description.AddressU       = texture_address_mode;
    sampler_description.AddressV       = texture_address_mode;
    sampler_description.AddressW       = texture_address_mode;
    sampler_description.MipLODBias     = 0.f;
    sampler_description.MaxAnisotropy  = 1;
    sampler_description.ComparisonFunc = D3D11_COMPARISON_ALWAYS;
    sampler_description.BorderColor[0] = border_color.r;
    sampler_description.BorderColor[1] = border_color.g;
    sampler_description.BorderColor[2] = border_color.b;
    sampler_description.BorderColor[3] = border_color.a;
    sampler_description.MinLOD         = 0.f;
    sampler_description.MaxLOD         = 0.f;
    
    D3D11_CALL(d3d_device->CreateTexture2D(&texture_description, &subresource, &texture->handle));
    D3D11_CALL(d3d_device->CreateShaderResourceView(texture->handle, null, &texture->view));
    D3D11_CALL(d3d_device->CreateSamplerState(&sampler_description, &texture->sampler));
    
    return Success;
}

void destroy_texture(Texture *texture) {
    if(texture->sampler) texture->sampler->Release();
    if(texture->view) texture->view->Release();
    if(texture->handle) texture->handle->Release();
    texture->sampler  = null;
    texture->view     = null;
    texture->handle   = null;
    texture->w        = 0;
    texture->h        = 0;
    texture->channels = 0;
}

void bind_texture(Texture *texture, s64 index_in_shader) {
    if(!texture->sampler || !texture->view) return;
    
    d3d_context->PSSetSamplers((UINT) index_in_shader, 1, &texture->sampler);
    d3d_context->PSSetShaderResources((UINT) index_in_shader, 1, &texture->view);
}



/* ------------------------------------------ Shader Constant Buffer ------------------------------------------ */

void create_shader_constant_buffer(Shader_Constant_Buffer *buffer, s64 size_in_bytes, void *initial_data) {
    D3D11_BUFFER_DESC description{};
    description.Usage          = D3D11_USAGE_DYNAMIC;
    description.ByteWidth      = align_to(size_in_bytes, 16, UINT);
    description.BindFlags      = D3D11_BIND_CONSTANT_BUFFER;
    description.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
    description.MiscFlags      = 0;
    
    D3D11_SUBRESOURCE_DATA subresource{};
    subresource.pSysMem = initial_data;
    
    D3D11_CALL(d3d_device->CreateBuffer(&description, (initial_data != null) ? &subresource : null, &buffer->handle));
    
    buffer->size_in_bytes = size_in_bytes;
}

void destroy_shader_constant_buffer(Shader_Constant_Buffer *buffer) {
    buffer->handle->Release();
    buffer->handle = null;
}

void update_shader_constant_buffer(Shader_Constant_Buffer *buffer, void *data) {
    D3D11_MAPPED_SUBRESOURCE subresource;
    D3D11_CALL(d3d_context->Map(buffer->handle, 0, D3D11_MAP_WRITE_DISCARD, 0, &subresource)); // This might fail if we haven't supplied CPU Write Access to this buffer!
    memcpy(subresource.pData, data, buffer->size_in_bytes);
    d3d_context->Unmap(buffer->handle, 0);
}

void bind_shader_constant_buffer(Shader_Constant_Buffer *buffer, s64 index_in_shader, Shader_Type shader_types) {
    if(shader_types & SHADER_Vertex) d3d_context->VSSetConstantBuffers((UINT) index_in_shader, 1, &buffer->handle);
    if(shader_types & SHADER_Pixel)  d3d_context->PSSetConstantBuffers((UINT) index_in_shader, 1, &buffer->handle);
}



/* -------------------------------------------------- Shader -------------------------------------------------- */

static
Error_Code create_shader_from_blob(Shader *shader, void *vertex, s64 vertex_size, void *pixel, s64 pixel_size, Shader_Input_Specification *inputs, s64 input_count) {
    Error_Code error_code = Success;
    
    HRESULT vertex_result = d3d_device->CreateVertexShader(vertex, vertex_size, null, &shader->vertex_shader);
    HRESULT pixel_result  = d3d_device->CreatePixelShader(pixel, pixel_size, null, &shader->pixel_shader);
    
    if(SUCCEEDED(vertex_result) && SUCCEEDED(pixel_result)) {
        foundation_assert(shader->vertex_shader != null && shader->pixel_shader != null);
        D3D11_INPUT_ELEMENT_DESC input_element_description[D3D11_MAX_SHADER_INPUTS];
        
        for(s64 i = 0; i < input_count; ++i) {                
            input_element_description[i] = {
                inputs[i].name, 0, d3d11_format(inputs[i].dimensions, D3D11_Float32), inputs[i].vertex_buffer_index, 0, D3D11_INPUT_PER_VERTEX_DATA, 0
            };
        }
        
        HRESULT result = d3d_device->CreateInputLayout(input_element_description, (UINT) input_count, vertex, vertex_size, &shader->input_layout);
        
        if(!SUCCEEDED(result)) {
            error_code = ERROR_D3D11_Invalid_Shader_Inputs;
            destroy_shader(shader);
        }
    } else {
        error_code = ERROR_Custom_Error_Message;
        set_custom_error_message("The shader uses unsupported bytecode on this device.");
        destroy_shader(shader);
    }
    
    return error_code;
}

Error_Code create_shader_from_file(Shader *shader, string file_path, Shader_Input_Specification *inputs, s64 input_count) {
    string file_content = os_read_file(Default_Allocator, file_path);
    if(!file_content.count) return ERROR_File_Not_Found;
    
    Error_Code error = create_shader_from_memory(shader, file_content, file_path, inputs, input_count);
    os_free_file_content(Default_Allocator, &file_content);
    return error;
}

Error_Code create_shader_from_memory(Shader *shader, string _string, string name, Shader_Input_Specification *inputs, s64 input_count) {
    Error_Code error_code = Success;
    ID3DBlob *error_blob = null, *vertex_blob = null, *pixel_blob = null;
    
    UINT compilation_flags = D3DCOMPILE_ENABLE_STRICTNESS;
    
#if FOUNDATION_DEVELOPER
    compilation_flags |= D3DCOMPILE_DEBUG;
#endif
    
    char *name_cstring = to_cstring(Default_Allocator, name);
    D3DCompile(_string.data, _string.count, name_cstring, null, D3D_COMPILE_STANDARD_FILE_INCLUDE, "vs_main", "vs_5_0", compilation_flags, 0, &vertex_blob, &error_blob);
    
    if(!error_blob) {
        // If the vertex shader already failed, then the error blob is already filled out and we would just
        // overwrite these error messages here, which we don't want to do.
        D3DCompile(_string.data, _string.count, name_cstring, null, D3D_COMPILE_STANDARD_FILE_INCLUDE, "ps_main", "ps_5_0", compilation_flags, 0, &pixel_blob, &error_blob);
    }
    
    if(vertex_blob && pixel_blob) {
        error_code = create_shader_from_blob(shader, vertex_blob->GetBufferPointer(), vertex_blob->GetBufferSize(), pixel_blob->GetBufferPointer(), pixel_blob->GetBufferSize(), inputs, input_count);
        vertex_blob->Release();
        pixel_blob->Release();
    } else {
        error_code = ERROR_Custom_Error_Message;
        set_custom_error_message((char *) error_blob->GetBufferPointer(), error_blob->GetBufferSize() - 2); // Don't include the null terminator nor the line ending at the end of this message
        destroy_shader(shader);
    }
    
    free_cstring(Default_Allocator, name_cstring);
    if(error_blob) error_blob->Release();
    
    return error_code;
}

Error_Code create_shader_from_compiled_file(Shader *shader, string file_path, Shader_Input_Specification *inputs, s64 input_count) {
    string file_content = os_read_file(Default_Allocator, file_path);
    if(!file_content.count) return ERROR_File_Not_Found;
    
    Error_Code error = create_shader_from_compiled_memory(shader, file_content, file_path, inputs, input_count);
    os_free_file_content(Default_Allocator, &file_content);
    return error;
}

Error_Code create_shader_from_compiled_memory(Shader *shader, string _string, string name, Shader_Input_Specification *inputs, s64 input_count) {
    s64 vertex_size_in_bytes = *(s64 *) &_string.data[0];
    s64 pixel_size_in_bytes = *(s64 *) &_string.data[8];
    void *vertex_pointer = &_string.data[16];
    void *pixel_pointer = &_string.data[16 + vertex_size_in_bytes];
    
    Error_Code error_code = create_shader_from_blob(shader, vertex_pointer, vertex_size_in_bytes, pixel_pointer, pixel_size_in_bytes, inputs, input_count);
    return error_code;
}

void destroy_shader(Shader *shader) {
    if(shader->vertex_shader) shader->vertex_shader->Release();
    if(shader->pixel_shader)  shader->pixel_shader->Release();
    if(shader->input_layout)  shader->input_layout->Release();
    
    shader->vertex_shader = null;
    shader->pixel_shader  = null;
    shader->input_layout  = null;
}

void bind_shader(Shader *shader) {
    if(!shader->vertex_shader || !shader->pixel_shader || !shader->input_layout) return;
    
    d3d_context->VSSetShader(shader->vertex_shader, null, 0);
    d3d_context->PSSetShader(shader->pixel_shader, null, 0);
    d3d_context->IASetInputLayout(shader->input_layout);
}


Compiled_Shader_Output compile_shader_from_file(string file_path) {
    string _string = os_read_file(Default_Allocator, file_path);
    if(!_string.count) {
        Compiled_Shader_Output output;
        output.error = ERROR_File_Not_Found;
        return output;
    }
    
    Compiled_Shader_Output output;
    output.error = Success;
    
    ID3DBlob *error_blob = null, *vertex_blob = null, *pixel_blob = null;
    
    UINT compilation_flags = D3DCOMPILE_ENABLE_STRICTNESS; // No debug here since this is probably baking the shader for shipping.

    char *cstring_name = to_cstring(Default_Allocator, file_path);
    defer { free_cstring(Default_Allocator, cstring_name); };
    
    D3DCompile(_string.data, _string.count, cstring_name, null, D3D_COMPILE_STANDARD_FILE_INCLUDE, "vs_main", "vs_5_0", compilation_flags, 0, &vertex_blob, &error_blob);
    
    if(!error_blob) {
        // If the vertex shader already failed, then the error blob is already filled out and we would just
        // overwrite these error messages here, which we don't want to do.
        D3DCompile(_string.data, _string.count, cstring_name, null, D3D_COMPILE_STANDARD_FILE_INCLUDE, "ps_main", "ps_5_0", compilation_flags, 0, &pixel_blob, &error_blob);
    }
    
    if(vertex_blob && pixel_blob) {
        output.vertex_size_in_bytes = vertex_blob->GetBufferSize();
        output.vertex_blob = Default_Allocator->allocate(output.vertex_size_in_bytes);
        output.pixel_size_in_bytes = pixel_blob->GetBufferSize();
        output.pixel_blob = Default_Allocator->allocate(output.pixel_size_in_bytes);
        
        memcpy(output.vertex_blob, vertex_blob->GetBufferPointer(), output.vertex_size_in_bytes);
        memcpy(output.pixel_blob, pixel_blob->GetBufferPointer(), output.pixel_size_in_bytes);
        
        vertex_blob->Release();
        pixel_blob->Release();
    } else {
        output.error = ERROR_Custom_Error_Message;
        set_custom_error_message((char *) error_blob->GetBufferPointer(), error_blob->GetBufferSize() - 2); // Don't include the null terminator nor the line ending at the end of this message
        error_blob->Release();
    }
    
    os_free_file_content(Default_Allocator, &_string);
    return output;
}

void destroy_compiled_shader_output(Compiled_Shader_Output *output) {
    Default_Allocator->deallocate(output->vertex_blob);
    output->vertex_blob = null;
    output->vertex_size_in_bytes = 0;
    
    Default_Allocator->deallocate(output->pixel_blob);
    output->pixel_blob = null;
    output->pixel_size_in_bytes = 0;
}



/* ----------------------------------------------- Frame Buffer ----------------------------------------------- */

static
void create_frame_buffer_color_attachment_internal(Frame_Buffer *frame_buffer, Frame_Buffer_Color_Attachment *attachment) {
    UINT bind_flags = D3D11_BIND_RENDER_TARGET;
    if(attachment->create_shader_view) bind_flags |= D3D11_BIND_SHADER_RESOURCE;
    
    D3D11_TEXTURE2D_DESC texture_description{};
    texture_description.Width              = attachment->w;
    texture_description.Height             = attachment->h;
    texture_description.MipLevels          = 1;
    texture_description.ArraySize          = 1;
    texture_description.Format             = (DXGI_FORMAT) attachment->format;
    texture_description.SampleDesc.Count   = frame_buffer->samples;
    texture_description.SampleDesc.Quality = 0;
    texture_description.Usage              = D3D11_USAGE_DEFAULT;
    texture_description.BindFlags          = bind_flags;
    texture_description.CPUAccessFlags     = 0;
    texture_description.MiscFlags          = 0;
    
    D3D11_RENDER_TARGET_VIEW_DESC view_description{};
    view_description.Format             = (DXGI_FORMAT) attachment->format;
    view_description.ViewDimension      = frame_buffer->samples > 1 ? D3D11_RTV_DIMENSION_TEXTURE2DMS : D3D11_RTV_DIMENSION_TEXTURE2D;
    view_description.Texture2D.MipSlice = 0;
    
    D3D11_CALL(d3d_device->CreateTexture2D(&texture_description, null, &attachment->texture));
    D3D11_CALL(d3d_device->CreateRenderTargetView(attachment->texture, &view_description, &attachment->render_view));
    
    if(attachment->create_shader_view) {
        D3D11_CALL(d3d_device->CreateShaderResourceView(attachment->texture, null, &attachment->shader_view));
        
        D3D11_FILTER filter = d3d11_filter(attachment->shader_view_hints);
        D3D11_TEXTURE_ADDRESS_MODE texture_address_mode = d3d11_texture_address_mode(attachment->shader_view_hints);
        
        D3D11_SAMPLER_DESC sampler_description{};
        sampler_description.Filter         = filter;
        sampler_description.AddressU       = texture_address_mode;
        sampler_description.AddressV       = texture_address_mode;
        sampler_description.AddressW       = texture_address_mode;
        sampler_description.MipLODBias     = 0.f;
        sampler_description.MaxAnisotropy  = 1;
        sampler_description.ComparisonFunc = D3D11_COMPARISON_ALWAYS;
        sampler_description.MinLOD         = 0.f;
        sampler_description.MaxLOD         = 0.f;
        D3D11_CALL(d3d_device->CreateSamplerState(&sampler_description, &attachment->sampler));
    } else {
        attachment->sampler = null;
        attachment->shader_view = null;
    }
}

static
void create_frame_buffer_depth_stencil_attachment_internal(Frame_Buffer *frame_buffer, Frame_Buffer_Depth_Attachment *attachment) {
    UINT bind_flags = D3D11_BIND_DEPTH_STENCIL;
    if(attachment->create_shader_view) bind_flags |= D3D11_BIND_SHADER_RESOURCE;
    
    D3D11_DEPTH_STENCILOP_DESC operations{};
    operations.StencilFailOp      = D3D11_STENCIL_OP_KEEP;
    operations.StencilDepthFailOp = D3D11_STENCIL_OP_KEEP;
    operations.StencilPassOp      = D3D11_STENCIL_OP_REPLACE;
    operations.StencilFunc        = D3D11_COMPARISON_ALWAYS;
    
    D3D11_DEPTH_STENCIL_DESC state_description{};
    state_description.DepthEnable      = TRUE;
    state_description.DepthWriteMask   = D3D11_DEPTH_WRITE_MASK_ALL;
    state_description.DepthFunc        = D3D11_COMPARISON_LESS_EQUAL;
    state_description.StencilEnable    = FALSE;
    state_description.StencilReadMask  = 0xff;
    state_description.StencilWriteMask = 0xff;
    state_description.FrontFace        = operations;
    state_description.BackFace         = operations;
    
    D3D11_TEXTURE2D_DESC texture_description{};
    texture_description.Width              = attachment->w;
    texture_description.Height             = attachment->h;
    texture_description.MipLevels          = 1;
    texture_description.ArraySize          = 1;
    texture_description.Format             = texture_format_for_depth_format((DXGI_FORMAT) attachment->format);
    texture_description.SampleDesc.Count   = frame_buffer->samples;
    texture_description.SampleDesc.Quality = 0;
    texture_description.Usage              = D3D11_USAGE_DEFAULT;
    texture_description.BindFlags          = bind_flags;
    texture_description.CPUAccessFlags     = 0;
    texture_description.MiscFlags          = 0;
    
    D3D11_DEPTH_STENCIL_VIEW_DESC render_view_description{};
    render_view_description.Format             = (DXGI_FORMAT) attachment->format;
    render_view_description.ViewDimension      = frame_buffer->samples > 1 ? D3D11_DSV_DIMENSION_TEXTURE2DMS : D3D11_DSV_DIMENSION_TEXTURE2D;
    render_view_description.Texture2D.MipSlice = 0;
    
    D3D11_CALL(d3d_device->CreateDepthStencilState(&state_description, &attachment->state));
    D3D11_CALL(d3d_device->CreateTexture2D(&texture_description, null, &attachment->texture));
    D3D11_CALL(d3d_device->CreateDepthStencilView(attachment->texture, &render_view_description, &attachment->render_view));
    
    if(attachment->create_shader_view) {
        D3D11_SHADER_RESOURCE_VIEW_DESC shader_view_description{};
        shader_view_description.Format = resource_format_for_depth_format((DXGI_FORMAT) attachment->format);
        shader_view_description.ViewDimension = frame_buffer->samples > 1 ? D3D11_SRV_DIMENSION_TEXTURE2DMS : D3D11_SRV_DIMENSION_TEXTURE2D;
        shader_view_description.Texture2D.MipLevels = 1;
        
        D3D11_CALL(d3d_device->CreateShaderResourceView(attachment->texture, &shader_view_description, &attachment->shader_view));
        
        D3D11_FILTER filter = d3d11_filter(attachment->shader_view_hints);
        D3D11_TEXTURE_ADDRESS_MODE texture_address_mode = d3d11_texture_address_mode(attachment->shader_view_hints);
        
        D3D11_SAMPLER_DESC sampler_description{};
        sampler_description.Filter         = filter;
        sampler_description.AddressU       = texture_address_mode;
        sampler_description.AddressV       = texture_address_mode;
        sampler_description.AddressW       = texture_address_mode;
        sampler_description.MipLODBias     = 0.f;
        sampler_description.MaxAnisotropy  = 1;
        sampler_description.ComparisonFunc = D3D11_COMPARISON_LESS_EQUAL;
        sampler_description.BorderColor[0] = attachment->border_color.r;
        sampler_description.BorderColor[1] = attachment->border_color.g;
        sampler_description.BorderColor[2] = attachment->border_color.b;
        sampler_description.BorderColor[3] = attachment->border_color.a;
        sampler_description.MinLOD         = 0.f;
        sampler_description.MaxLOD         = 0.f;
        D3D11_CALL(d3d_device->CreateSamplerState(&sampler_description, &attachment->sampler));        
    } else {
        attachment->shader_view = null;
        attachment->sampler     = null;
    }
}

void create_frame_buffer(Frame_Buffer *frame_buffer, u8 samples) {
    frame_buffer->samples     = samples;
    frame_buffer->color_count = 0;
    frame_buffer->has_depth   = 0;
}

void destroy_frame_buffer(Frame_Buffer *frame_buffer) {
    for(s64 i = 0; i < frame_buffer->color_count; ++i) {
        if(frame_buffer->colors[i].sampler) frame_buffer->colors[i].sampler->Release();
        if(frame_buffer->colors[i].shader_view) frame_buffer->colors[i].shader_view->Release();
        frame_buffer->colors[i].render_view->Release();
        frame_buffer->colors[i].texture->Release();
    }
    
    frame_buffer->color_count = 0;
    
    if(frame_buffer->has_depth) {
        frame_buffer->depth.texture->Release();
        frame_buffer->depth.render_view->Release();
        frame_buffer->depth.state->Release();
        if(frame_buffer->depth.shader_view) frame_buffer->depth.shader_view->Release();
        if(frame_buffer->depth.sampler) frame_buffer->depth.sampler->Release();
    }
    
    frame_buffer->has_depth = false;
}

void create_frame_buffer_color_attachment(Frame_Buffer *frame_buffer, s32 w, s32 h, Frame_Buffer_Color_Format format, b8 create_shader_view, Texture_Hints shader_view_hints, Border_Color border_color) {
    foundation_assert(frame_buffer->color_count < ARRAY_COUNT(frame_buffer->colors));
    
    Frame_Buffer_Color_Attachment *attachment = &frame_buffer->colors[frame_buffer->color_count];
    attachment->create_shader_view = create_shader_view;
    attachment->w = w;
    attachment->h = h;
    attachment->format = d3d11_format(format);
    attachment->shader_view_hints = shader_view_hints;
    attachment->border_color = border_color;
    
    create_frame_buffer_color_attachment_internal(frame_buffer, attachment);
    
    ++frame_buffer->color_count;
}

void create_frame_buffer_depth_stencil_attachment(Frame_Buffer *frame_buffer, s32 w, s32 h, b8 create_shader_view, Texture_Hints shader_view_hints, Border_Color border_color) {
    foundation_assert(frame_buffer->has_depth == false);
    
    frame_buffer->depth.create_shader_view = create_shader_view;
    frame_buffer->depth.w = w;
    frame_buffer->depth.h = h;
    frame_buffer->depth.format = DXGI_FORMAT_D24_UNORM_S8_UINT;
    frame_buffer->depth.shader_view_hints = shader_view_hints;
    frame_buffer->depth.border_color = border_color;
    
    create_frame_buffer_depth_stencil_attachment_internal(frame_buffer, &frame_buffer->depth);
    
    frame_buffer->has_depth = true;
}

void resize_frame_buffer_color_attachment(Frame_Buffer *frame_buffer, s64 index, s32 w, s32 h) {
    foundation_assert(index >= 0 && index < frame_buffer->color_count);
    
    Frame_Buffer_Color_Attachment *attachment = &frame_buffer->colors[index];
    
    if(attachment->shader_view) attachment->shader_view->Release();
    if(attachment->sampler) attachment->sampler->Release();
    attachment->render_view->Release();
    attachment->texture->Release();
    
    attachment->w = w;
    attachment->h = h;
    
    create_frame_buffer_color_attachment_internal(frame_buffer, attachment);
}

void resize_frame_buffer_depth_stencil_attachment(Frame_Buffer *frame_buffer, s32 w, s32 h) {
    foundation_assert(frame_buffer->has_depth == true);
    
    frame_buffer->depth.render_view->Release();
    frame_buffer->depth.state->Release();
    frame_buffer->depth.texture->Release();
    if(frame_buffer->depth.shader_view) frame_buffer->depth.shader_view->Release();
    if(frame_buffer->depth.sampler) frame_buffer->depth.sampler->Release();
    
    frame_buffer->depth.w = w;
    frame_buffer->depth.h = h;
    
    create_frame_buffer_depth_stencil_attachment_internal(frame_buffer, &frame_buffer->depth);
}

void resize_frame_buffer(Frame_Buffer *frame_buffer, s32 w, s32 h) {
    for(s64 i = 0; i < frame_buffer->color_count; ++i) resize_frame_buffer_color_attachment(frame_buffer, i, w, h);
    if(frame_buffer->has_depth) resize_frame_buffer_depth_stencil_attachment(frame_buffer, w, h);
}

void bind_frame_buffer_color_attachment_to_shader(Frame_Buffer *frame_buffer, s64 attachment_index, s64 index_in_shader) {
    foundation_assert(attachment_index >= 0 && attachment_index < frame_buffer->color_count);
    Frame_Buffer_Color_Attachment *attachment = &frame_buffer->colors[attachment_index];
    d3d_context->PSSetSamplers((UINT) index_in_shader, 1, &attachment->sampler);
    d3d_context->PSSetShaderResources((UINT) index_in_shader, 1, &attachment->shader_view);
}

void bind_frame_buffer(Frame_Buffer *frame_buffer) {
    D3D11_VIEWPORT viewports[ARRAY_COUNT(frame_buffer->colors)];
    UINT viewport_count;
    
    if(frame_buffer->color_count) {
        for(s64 i = 0; i < frame_buffer->color_count; ++i) {
            viewports[i] = { 0.f, 0.f, (f32) frame_buffer->colors[i].w, (f32) frame_buffer->colors[i].h, 0.f, 1.f };
        }
        viewport_count = (UINT) frame_buffer->color_count;
    } else if(frame_buffer->has_depth) {
        viewports[0] = { 0.f, 0.f, (f32) frame_buffer->depth.w, (f32) frame_buffer->depth.h, 0.f, 1.f };
        viewport_count = 1;
    } else {
        viewport_count = 0;
    }
    
    d3d_context->RSSetViewports(viewport_count, viewports);
    
    ID3D11RenderTargetView *views[ARRAY_COUNT(frame_buffer->colors)];
    for(s64 i = 0; i < frame_buffer->color_count; ++i) {
        views[i] = frame_buffer->colors[i].render_view;
    }
    d3d_context->OMSetRenderTargets((UINT) frame_buffer->color_count, views, (frame_buffer->has_depth) ? frame_buffer->depth.render_view : null);
    
    if(frame_buffer->has_depth) d3d_context->OMSetDepthStencilState(frame_buffer->depth.state, 1);
}

void clear_frame_buffer(Frame_Buffer *frame_buffer, f32 r, f32 g, f32 b, f32 a, f32 depth) {
    f32 color_array[4];
    color_array[0] = r;
    color_array[1] = g;
    color_array[2] = b;
    color_array[3] = 1.f;
    
    for(s64 i = 0; i < frame_buffer->color_count; ++i) {
        d3d_context->ClearRenderTargetView(frame_buffer->colors[i].render_view, color_array);
    }
    
    u8 stencil = 0;
    
    if(frame_buffer->has_depth) d3d_context->ClearDepthStencilView(frame_buffer->depth.render_view, D3D11_CLEAR_DEPTH, depth, stencil);
}

void clear_frame_buffer_depth_stencil(Frame_Buffer *frame_buffer, f32 depth) {
    u8 stencil = 0;

    if(frame_buffer->has_depth) d3d_context->ClearDepthStencilView(frame_buffer->depth.render_view, D3D11_CLEAR_DEPTH, depth, stencil);
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
            foundation_assert(dst->depth.format == src->depth.format);
            d3d_context->CopyResource(dst->depth.texture, src->depth.texture);
        }
    } else {
        for(s64 i = 0; i < dst->color_count; ++i) {
            foundation_assert(dst->colors[i].w == src->colors[i].w && dst->colors[i].h == src->colors[i].h);
            foundation_assert(dst->colors[i].format == src->colors[i].format);
            d3d_context->ResolveSubresource(dst->colors[i].texture, 0, src->colors[i].texture, 0, (DXGI_FORMAT) dst->colors[i].format);
        }
        
        if(dst->has_depth && src->has_depth) {
            foundation_assert(dst->depth.format == src->depth.format);
            d3d_context->ResolveSubresource(dst->depth.texture, 0, src->depth.texture, 0, (DXGI_FORMAT) dst->depth.format);
        }
    }
}


Texture texture_wrapper_for_frame_buffer_color_attachment(Frame_Buffer *frame_buffer, s64 index) {
    foundation_assert(index >= 0 && index < frame_buffer->color_count);
    Frame_Buffer_Color_Attachment *attachment = &frame_buffer->colors[index];
    
    foundation_assert(attachment->create_shader_view);
    
    Texture texture;
    texture.w        = attachment->w;
    texture.h        = attachment->h;
    texture.channels = get_channels_for_d3d11_format((DXGI_FORMAT) attachment->format);
    texture.handle   = attachment->texture;
    texture.sampler  = attachment->sampler;
    texture.view     = attachment->shader_view;
    return texture;
}

Texture texture_wrapper_for_frame_buffer_depth_attachment(Frame_Buffer *frame_buffer) {
    foundation_assert(frame_buffer->has_depth);
    Frame_Buffer_Depth_Attachment *attachment = &frame_buffer->depth;
    
    foundation_assert(attachment->create_shader_view);
    
    Texture texture;
    texture.w        = attachment->w;
    texture.h        = attachment->h;
    texture.channels = get_channels_for_d3d11_format((DXGI_FORMAT) attachment->format);
    texture.handle   = attachment->texture;
    texture.sampler  = attachment->sampler;
    texture.view     = attachment->shader_view;
    return texture;
}



/* ---------------------------------------------- Pipeline State ---------------------------------------------- */

void create_pipeline_state(Pipeline_State *state) {
    D3D11_CULL_MODE cull_mode;
    switch(state->cull_mode) {
        case CULL_Disabled:    cull_mode = D3D11_CULL_NONE;  break;
        case CULL_Back_Faces:  cull_mode = D3D11_CULL_BACK;  break;
        case CULL_Front_Faces: cull_mode = D3D11_CULL_FRONT; break;
        default: cull_mode = D3D11_CULL_NONE; break;
    }
    
    D3D11_RASTERIZER_DESC rasterizer_description{};
    rasterizer_description.FillMode              = D3D11_FILL_SOLID;
    rasterizer_description.CullMode              = cull_mode;
    rasterizer_description.FrontCounterClockwise = TRUE;
    rasterizer_description.DepthBias             = 0;
    rasterizer_description.DepthBiasClamp        = 0;
    rasterizer_description.SlopeScaledDepthBias  = 0;
    rasterizer_description.DepthClipEnable       = state->enable_depth_test;
    rasterizer_description.ScissorEnable         = state->enable_scissors;
    rasterizer_description.MultisampleEnable     = state->enable_multisample;
    rasterizer_description.AntialiasedLineEnable = FALSE;
    D3D11_CALL(d3d_device->CreateRasterizerState(&rasterizer_description, &state->rasterizer));
    
    D3D11_BLEND_DESC blend_description{};
    blend_description.AlphaToCoverageEnable  = FALSE;
    blend_description.IndependentBlendEnable = FALSE; // All render targets should use the same blend state.
    switch(state->blend_mode) {
        case BLEND_Additive:
        blend_description.RenderTarget[0].BlendEnable           = TRUE;
        blend_description.RenderTarget[0].SrcBlend              = D3D11_BLEND_SRC_ALPHA;
        blend_description.RenderTarget[0].DestBlend             = D3D11_BLEND_DEST_ALPHA;
        blend_description.RenderTarget[0].BlendOp               = D3D11_BLEND_OP_ADD;
        blend_description.RenderTarget[0].SrcBlendAlpha         = D3D11_BLEND_SRC_ALPHA;
        blend_description.RenderTarget[0].DestBlendAlpha        = D3D11_BLEND_DEST_ALPHA;
        blend_description.RenderTarget[0].BlendOpAlpha          = D3D11_BLEND_OP_ADD;
        blend_description.RenderTarget[0].RenderTargetWriteMask = D3D11_COLOR_WRITE_ENABLE_ALL;
        break;
        
        case BLEND_Default:
        blend_description.RenderTarget[0].BlendEnable           = TRUE;
        blend_description.RenderTarget[0].SrcBlend              = D3D11_BLEND_SRC_ALPHA;
        blend_description.RenderTarget[0].DestBlend             = D3D11_BLEND_INV_SRC_ALPHA;
        blend_description.RenderTarget[0].BlendOp               = D3D11_BLEND_OP_ADD;
        blend_description.RenderTarget[0].SrcBlendAlpha         = D3D11_BLEND_SRC_ALPHA;
        blend_description.RenderTarget[0].DestBlendAlpha        = D3D11_BLEND_INV_SRC_ALPHA;
        blend_description.RenderTarget[0].BlendOpAlpha          = D3D11_BLEND_OP_ADD;
        blend_description.RenderTarget[0].RenderTargetWriteMask = D3D11_COLOR_WRITE_ENABLE_ALL;
        break;
        
        default:
        blend_description.RenderTarget[0].BlendEnable = FALSE;
        blend_description.RenderTarget[0].RenderTargetWriteMask = D3D11_COLOR_WRITE_ENABLE_ALL;
        break;
    }
    
    D3D11_CALL(d3d_device->CreateBlendState(&blend_description, &state->blender));
}

void destroy_pipeline_state(Pipeline_State *state) {
    state->blender->Release();
    state->rasterizer->Release();
    
    state->blender    = null;
    state->rasterizer = null;
}

void bind_pipeline_state(Pipeline_State *state) {
    d3d_context->OMSetBlendState(state->blender, null, 0xffffffff);
    d3d_context->RSSetState(state->rasterizer);
}
