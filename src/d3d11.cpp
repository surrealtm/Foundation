#include "d3d11.h"
#include "window.h"

#include <d3d11.h>
#include <dxgi1_2.h>

#define D3D11_CALL(expr) assert(SUCCEEDED(expr))

struct Window_D3D11_State {
    IDXGISwapChain1 *swapchain;
    ID3D11Texture2D *backbuffer;
    ID3D11RenderTargetView *backbuffer_view;
};

static_assert(sizeof(Window_D3D11_State) <= sizeof(Window::graphics_data), "Window_D3D11_State is bigger than expected.");



/* ------------------------------------------------- Globals ------------------------------------------------- */

s64 d3d_device_usage_count;
IDXGIDevice2  *dxgi_device  = null;
IDXGIAdapter  *dxgi_adapter = null;
IDXGIFactory2 *dxgi_factory = null;
ID3D11Device *d3d_device = null;
ID3D11DeviceContext *d3d_context = null;
D3D_FEATURE_LEVEL d3d_feature_level;



/* ------------------------------------------------ Setup Code ------------------------------------------------ */

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
    swapchain_description.Stereo             = true;
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

    d3d_context->OMSetRenderTargets(1, &d3d11->backbuffer_view, NULL);
}

void destroy_d3d11_context(Window *window) {
    Window_D3D11_State *d3d11 = (Window_D3D11_State *) window->graphics_data;
    d3d11->backbuffer_view->Release();
    d3d11->backbuffer->Release();
    d3d11->swapchain->Release();
    
    destroy_d3d11();
}

void swap_d3d11_buffers(Window *window) {
    Window_D3D11_State *d3d11 = (Window_D3D11_State *) window->graphics_data;
    d3d11->swapchain->Present(1, 0);
}



/* ---------------------------------------------- Vertex Buffer ---------------------------------------------- */

void create_vertex_buffer(Vertex_Buffer *buffer, f32 *data, u64 float_count, u8 dimensions, Vertex_Buffer_Topology topology) {
    assert(float_count % dimensions == 0, "Expected byte_count to be a multiple of dimensions.");

    buffer->primitive_count = float_count / dimensions;
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
    buffer->handle          = null;
    buffer->primitive_count = 0;
    buffer->topology        = VERTEX_BUFFER_Undefined;
}
