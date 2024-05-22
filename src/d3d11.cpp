#include "d3d11.h"
#include "window.h"
#include "os_specific.h"
#include "memutils.h"

#undef null // comdef.h has many parameters called 'null'... For fucks sake.

#include <d3d11.h>
#include <d3dcompiler.h>
#include <dxgi1_2.h>
#include <comdef.h>

#define null 0

#define D3D11_CALL(expr) d3d11_call_wrapper(expr, __FILE__ "," STRINGIFY(__LINE__) ": " STRINGIFY(expr))

struct Window_D3D11_State {
    IDXGISwapChain1 *swapchain;
    ID3D11Texture2D *backbuffer;
    ID3D11RenderTargetView *backbuffer_view;
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
    foundation_do_assertion_fail(assertion_text, "%.*s", (u32) error_string_length, error_string);
}

static
void create_d3d11_device() {
    ++d3d_device_usage_count;
    if(d3d_device != null) return;

    D3D_DRIVER_TYPE driver = D3D_DRIVER_TYPE_HARDWARE;
    UINT flags = D3D11_CREATE_DEVICE_SINGLETHREADED | D3D11_CREATE_DEVICE_DEBUG;
    UINT sdk = D3D11_SDK_VERSION;
    
    D3D11_CALL(D3D11CreateDevice(NULL, driver, NULL, flags, NULL, 0, sdk, &d3d_device, &d3d_feature_level, &d3d_context));

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

void create_d3d11_context(Window *window) {
    create_d3d11_device();

    Window_D3D11_State *d3d11 = (Window_D3D11_State *) window->graphics_data;
    
    DXGI_SWAP_CHAIN_DESC1 swapchain_description{};
    swapchain_description.Width              = window->w;
    swapchain_description.Height             = window->h;
    swapchain_description.Format             = DXGI_FORMAT_R8G8B8A8_UNORM;
    swapchain_description.Stereo             = FALSE;
    swapchain_description.SampleDesc.Count   = 1;
    swapchain_description.SampleDesc.Quality = 0;
    swapchain_description.BufferUsage        = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    swapchain_description.BufferCount        = 2;
    swapchain_description.Scaling            = DXGI_SCALING_STRETCH;
    swapchain_description.SwapEffect         = DXGI_SWAP_EFFECT_FLIP_SEQUENTIAL;
    swapchain_description.AlphaMode          = DXGI_ALPHA_MODE_UNSPECIFIED;
    swapchain_description.Flags              = 0;

    D3D11_CALL(dxgi_factory->CreateSwapChainForHwnd(d3d_device, (HWND) window_extract_hwnd(window), &swapchain_description, NULL, NULL, &d3d11->swapchain));
    D3D11_CALL(d3d11->swapchain->GetBuffer(0, __uuidof(ID3D11Texture2D), (void **) &d3d11->backbuffer));
    D3D11_CALL(d3d_device->CreateRenderTargetView(d3d11->backbuffer, 0, &d3d11->backbuffer_view));
}

void destroy_d3d11_context(Window *window) {
    Window_D3D11_State *d3d11 = (Window_D3D11_State *) window->graphics_data;
    d3d11->backbuffer_view->Release();
    d3d11->backbuffer->Release();
    d3d11->swapchain->Release();
    
    destroy_d3d11();
}

void clear_d3d11_buffer(Window *window, u8 r, u8 g, u8 b) {
    Window_D3D11_State *d3d11 = (Window_D3D11_State *) window->graphics_data;

    d3d_context->OMSetRenderTargets(1, &d3d11->backbuffer_view, NULL);
    
    D3D11_VIEWPORT viewport = {
        0.f, 0.f,
        (f32) window->w, (f32) window->h,
        0.f, 1.f
    };
    d3d_context->RSSetViewports(1, &viewport);

    f32 color_array[4];
    color_array[0] = (f32) r / 255.f;
    color_array[1] = (f32) g / 255.f;
    color_array[2] = (f32) b / 255.f;
    color_array[3] = 1.f;
    d3d_context->ClearRenderTargetView(d3d11->backbuffer_view, &color_array[0]);
};

void swap_d3d11_buffers(Window *window) {
    Window_D3D11_State *d3d11 = (Window_D3D11_State *) window->graphics_data;
    d3d11->swapchain->Present(1, 0);
}



/* ---------------------------------------------- Vertex Buffer ---------------------------------------------- */

void create_vertex_buffer(Vertex_Buffer *buffer, f32 *data, u64 float_count, u8 dimensions, Vertex_Buffer_Topology topology) {
    foundation_assert(float_count % dimensions == 0, "Expected byte_count to be a multiple of dimensions."); // comdef.h overrides 'assert'...

    buffer->vertex_count = float_count / dimensions;
    buffer->dimensions = dimensions;
    buffer->topology = topology;

    D3D11_BUFFER_DESC buffer_description{};
    buffer_description.ByteWidth = (UINT) (float_count * sizeof(f32));
    buffer_description.Usage     = D3D11_USAGE_DEFAULT;
    buffer_description.BindFlags = D3D11_BIND_VERTEX_BUFFER;

    D3D11_SUBRESOURCE_DATA subresource{};
    subresource.pSysMem = data;

    D3D11_CALL(d3d_device->CreateBuffer(&buffer_description, &subresource, &buffer->handle));
}

void destroy_vertex_buffer(Vertex_Buffer *buffer) {
    buffer->handle->Release();
    buffer->handle       = null;
    buffer->vertex_count = 0;
    buffer->topology     = VERTEX_BUFFER_Undefined;
}

void bind_vertex_buffer(Vertex_Buffer *buffer) {
    UINT stride = buffer->dimensions * sizeof(f32), offset = 0;
    d3d_context->IASetPrimitiveTopology((D3D11_PRIMITIVE_TOPOLOGY) buffer->topology);
    d3d_context->IASetVertexBuffers(0, 1, &buffer->handle, &stride, &offset);
}

void draw_vertex_buffer(Vertex_Buffer *buffer) {
    d3d_context->Draw((UINT) buffer->vertex_count, 0);
}



/* -------------------------------------------------- Shader -------------------------------------------------- */

void create_shader_from_file(Shader *shader, string file_path) {
    ID3DBlob *error_blob = null;

    string file_content = os_read_file(Default_Allocator, file_path);
    if(!file_content.count) {
        foundation_error("Failed to load shader '%.*s' from disk.", (u32) file_path.count, file_path.data);
        return;
    }

    char *file_path_cstring = to_cstring(Default_Allocator, file_path);
    D3DCompile(file_content.data, file_content.count, file_path_cstring, NULL, D3D_COMPILE_STANDARD_FILE_INCLUDE, "vs_main", "vs_5_0", D3DCOMPILE_ENABLE_STRICTNESS | D3DCOMPILE_DEBUG, 0, &shader->vertex_blob, &error_blob);
    D3DCompile(file_content.data, file_content.count, file_path_cstring, NULL, D3D_COMPILE_STANDARD_FILE_INCLUDE, "ps_main", "ps_5_0", D3DCOMPILE_ENABLE_STRICTNESS | D3DCOMPILE_DEBUG, 0, &shader->pixel_blob, &error_blob);

    if(shader->vertex_blob && shader->pixel_blob) {
        D3D11_CALL(d3d_device->CreateVertexShader(shader->vertex_blob->GetBufferPointer(), shader->vertex_blob->GetBufferSize(), null, &shader->vertex_shader));
        D3D11_CALL(d3d_device->CreatePixelShader(shader->pixel_blob->GetBufferPointer(), shader->pixel_blob->GetBufferSize(), null, &shader->pixel_shader));

        if(shader->vertex_shader && shader->pixel_shader) {
            D3D11_INPUT_ELEMENT_DESC input_element_description[] = {
                { "POSITION", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0 },
            };

            D3D11_CALL(d3d_device->CreateInputLayout(input_element_description, ARRAYSIZE(input_element_description), shader->vertex_blob->GetBufferPointer(), shader->vertex_blob->GetBufferSize(), &shader->input_layout));
        } else {
            foundation_error("Failed to create shader '%.*s'.", (u32) file_path.count, file_path.data);
            destroy_shader(shader);
        }
    } else {
        assert(error_blob != null);
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
    d3d_context->VSSetShader(shader->vertex_shader, NULL, 0);
    d3d_context->PSSetShader(shader->pixel_shader, NULL, 0);
    d3d_context->IASetInputLayout(shader->input_layout);
}



/* ---------------------------------------------- Pipeline State ---------------------------------------------- */

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
