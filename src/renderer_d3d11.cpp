#include "pch.h"
#include "renderer.h"
#include "mesh.h"

#include <d3d11.h>
#include <dxgi1_6.h>
#include <stdio.h>
#include <stdlib.h>
#include <float.h>

#include <imgui.h>

#ifdef RENDER_D3D11
#include <imgui_impl_dx11.h>
#endif

#define SafeRelease(ptr) if (ptr) { ptr->Release(); ptr = NULL; }

static bool should_vsync;

extern bool init_shaders();

static bool shadow_maps_created = false;

static ID3D11Device *device;
static ID3D11DeviceContext *device_context;
static IDXGISwapChain *swap_chain;
static u32 swap_chain_flags;

static ID3D11SamplerState *sampler_point        = NULL;
static ID3D11SamplerState *sampler_linear       = NULL;
static ID3D11SamplerState *sampler_point_clamp  = NULL;
static ID3D11SamplerState *sampler_linear_clamp = NULL;

/*
static Gpu_Buffer per_scene_cb;
static Gpu_Buffer per_object_cb;
*/

static Gpu_Buffer fullscreen_quad_vb;
static Gpu_Buffer fullscreen_quad_ib;
static ID3D11InputLayout *quad_input_layout = NULL;
static ID3D11RasterizerState *quad_rasterizer_state = NULL;
static ID3D11DepthStencilState *quad_depth_stencil_state = NULL;
static ID3D11BlendState *quad_blend_enabled = NULL;

static ID3D11InputLayout *mesh_vertex_input_layout = NULL;
static ID3D11RasterizerState *rasterizer_state_for_mesh_rendering = NULL;
static ID3D11DepthStencilState *depth_stencil_state_for_mesh_rendering = NULL;
static ID3D11BlendState *opaque_mesh_blend_disabled = NULL;

static ID3D11BlendState *no_render_target_write_blend_state = NULL;
static ID3D11RasterizerState *rasterizer_state_for_shadow_rendering = NULL;

static void init_back_buffer() {
    swap_chain->GetBuffer(0, IID_PPV_ARGS(&back_buffer.texture));

    D3D11_RENDER_TARGET_VIEW_DESC rtv_desc = {};
    rtv_desc.Format        = DXGI_FORMAT_B8G8R8A8_UNORM_SRGB;
    rtv_desc.ViewDimension = D3D11_RTV_DIMENSION_TEXTURE2D;
    device->CreateRenderTargetView(back_buffer.texture, &rtv_desc, &back_buffer.rtv);

    D3D11_TEXTURE2D_DESC texture_desc = {};
    texture_desc.Width              = platform_window_width;
    texture_desc.Height             = platform_window_height;
    texture_desc.MipLevels          = 1;
    texture_desc.ArraySize          = 1;
    texture_desc.Format             = DXGI_FORMAT_D32_FLOAT;
    texture_desc.SampleDesc.Count   = 1;
    texture_desc.SampleDesc.Quality = 0;
    texture_desc.Usage              = D3D11_USAGE_DEFAULT;
    texture_desc.BindFlags          = D3D11_BIND_DEPTH_STENCIL;
    device->CreateTexture2D(&texture_desc, NULL, &offscreen_depth_target.texture);

    D3D11_DEPTH_STENCIL_VIEW_DESC dsv_desc = {};
    dsv_desc.Format        = texture_desc.Format;
    dsv_desc.ViewDimension = D3D11_DSV_DIMENSION_TEXTURE2D;
    device->CreateDepthStencilView(offscreen_depth_target.texture, &dsv_desc, &offscreen_depth_target.dsv);

    if (!shadow_maps_created) {
        for (int i = 0; i < MAX_SHADOW_CASCADES; i++) {
            texture_desc.Width     = SHADOW_MAP_WIDTH;
            texture_desc.Height    = SHADOW_MAP_HEIGHT;
            texture_desc.Format    = DXGI_FORMAT_R32_TYPELESS;
            texture_desc.BindFlags |= D3D11_BIND_SHADER_RESOURCE;
            device->CreateTexture2D(&texture_desc, NULL, &shadow_map_targets[i].texture);

            device->CreateDepthStencilView(shadow_map_targets[i].texture, &dsv_desc, &shadow_map_targets[i].dsv);

            D3D11_SHADER_RESOURCE_VIEW_DESC srv_desc = {};
            srv_desc.Format                    = DXGI_FORMAT_R32_FLOAT;
            srv_desc.ViewDimension             = D3D11_SRV_DIMENSION_TEXTURE2D;
            srv_desc.Texture2D.MostDetailedMip = 0;
            srv_desc.Texture2D.MipLevels       = 1;
            device->CreateShaderResourceView(shadow_map_targets[i].texture, &srv_desc, &shadow_map_targets[i].srv);
        }

        shadow_maps_created = true;
    }

    bool do_hdr = true;
    
    texture_desc.Width     = platform_window_width;
    texture_desc.Height    = platform_window_height;
    if (do_hdr) {
        texture_desc.Format    = DXGI_FORMAT_R16G16B16A16_FLOAT;
    } else {
        texture_desc.Format    = DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;
    }
    texture_desc.BindFlags = D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE;
    device->CreateTexture2D(&texture_desc, NULL, &offscreen_render_target.texture);
    device->CreateTexture2D(&texture_desc, NULL, &offscreen_bloom_target.texture);
    device->CreateTexture2D(&texture_desc, NULL, &ping_pong_render_targets[0].texture);
    device->CreateTexture2D(&texture_desc, NULL, &ping_pong_render_targets[1].texture);
    
    rtv_desc.Format = texture_desc.Format;
    device->CreateRenderTargetView(offscreen_render_target.texture, &rtv_desc, &offscreen_render_target.rtv);
    device->CreateRenderTargetView(offscreen_bloom_target.texture, &rtv_desc, &offscreen_bloom_target.rtv);
    device->CreateRenderTargetView(ping_pong_render_targets[0].texture, &rtv_desc, &ping_pong_render_targets[0].rtv);
    device->CreateRenderTargetView(ping_pong_render_targets[1].texture, &rtv_desc, &ping_pong_render_targets[1].rtv);

    D3D11_SHADER_RESOURCE_VIEW_DESC srv_desc = {};
    srv_desc.Format                    = texture_desc.Format;
    srv_desc.ViewDimension             = D3D11_SRV_DIMENSION_TEXTURE2D;
    srv_desc.Texture2D.MostDetailedMip = 0;
    srv_desc.Texture2D.MipLevels       = 1;
    device->CreateShaderResourceView(offscreen_render_target.texture, &srv_desc, &offscreen_render_target.srv);
    device->CreateShaderResourceView(offscreen_bloom_target.texture, &srv_desc, &offscreen_bloom_target.srv);
    device->CreateShaderResourceView(ping_pong_render_targets[0].texture, &srv_desc, &ping_pong_render_targets[0].srv);
    device->CreateShaderResourceView(ping_pong_render_targets[1].texture, &srv_desc, &ping_pong_render_targets[1].srv);
}

static void release_back_buffer() {
    release_texture(&ping_pong_render_targets[0]);
    release_texture(&ping_pong_render_targets[1]);
    release_texture(&offscreen_depth_target);
    release_texture(&offscreen_bloom_target);
    release_texture(&offscreen_render_target);
    release_texture(&back_buffer);
}

void init_renderer(bool vsync) {
    should_vsync = vsync;
    if (!should_vsync) {
        swap_chain_flags = DXGI_SWAP_CHAIN_FLAG_ALLOW_TEARING;
    }
    
    DXGI_SWAP_CHAIN_DESC swap_chain_desc = {};
    swap_chain_desc.BufferDesc.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
    swap_chain_desc.SampleDesc.Count  = 1;
    swap_chain_desc.BufferUsage       = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    swap_chain_desc.BufferCount       = 2;
    swap_chain_desc.OutputWindow      = (HWND)platform_window_get_native();
    swap_chain_desc.Windowed          = TRUE;
    swap_chain_desc.SwapEffect        = DXGI_SWAP_EFFECT_FLIP_DISCARD;
    swap_chain_desc.Flags             = swap_chain_flags;

    IDXGIFactory6 *factory = NULL;
    defer { SafeRelease(factory); };
    CreateDXGIFactory1(IID_PPV_ARGS(&factory));

    IDXGIAdapter1 *adapter = NULL;
    defer { SafeRelease(adapter); };
    
    factory->EnumAdapterByGpuPreference(0, DXGI_GPU_PREFERENCE_HIGH_PERFORMANCE, IID_PPV_ARGS(&adapter));
    
    D3D_FEATURE_LEVEL feature_levels[] = { D3D_FEATURE_LEVEL_11_0 };

    UINT create_flags = D3D11_CREATE_DEVICE_BGRA_SUPPORT;
#ifdef BUILD_DEBUG
    create_flags |= D3D11_CREATE_DEVICE_DEBUG;
#endif
    
    D3D11CreateDeviceAndSwapChain(adapter, D3D_DRIVER_TYPE_UNKNOWN, NULL, create_flags, feature_levels, ArrayCount(feature_levels), D3D11_SDK_VERSION, &swap_chain_desc, &swap_chain, &device, NULL, &device_context);

    Assert(swap_chain);
    Assert(device);
    Assert(device_context);

    immediate_cb.context = device_context;

    init_back_buffer();

    if (!init_shaders()) {
        exit(1);
    }

    //
    // Create fullscreen quad resources
    //
    {
        Quad_Vertex vertices[] = {
            { { -1.0f, -1.0f }, { 1, 1, 1, 1 }, { 0.0f, 1.0f } },
            { { +1.0f, -1.0f }, { 1, 1, 1, 1 }, { 1.0f, 1.0f } },
            { { +1.0f, +1.0f }, { 1, 1, 1, 1 }, { 1.0f, 0.0f } },
            { { -1.0f, +1.0f }, { 1, 1, 1, 1 }, { 0.0f, 0.0f } },
        };

        u32 indices[] = {
            0, 1, 2,
            0, 2, 3,
        };
        
        create_gpu_buffer(&fullscreen_quad_vb, GPU_BUFFER_TYPE_VERTEX, sizeof(vertices), sizeof(Quad_Vertex), vertices, false);
        create_gpu_buffer(&fullscreen_quad_ib, GPU_BUFFER_TYPE_INDEX, sizeof(indices), 0, indices, false);
    }

    //
    // Create blend states
    //
    {
        D3D11_BLEND_DESC blend_desc = {};
        blend_desc.RenderTarget[0].BlendEnable = FALSE;
        blend_desc.RenderTarget[0].RenderTargetWriteMask = 0;
        blend_desc.RenderTarget[0].BlendOp = D3D11_BLEND_OP_ADD;
        blend_desc.RenderTarget[0].SrcBlend = D3D11_BLEND_SRC_ALPHA;
        blend_desc.RenderTarget[0].DestBlend = D3D11_BLEND_INV_SRC_ALPHA;
        blend_desc.RenderTarget[0].BlendOpAlpha = D3D11_BLEND_OP_ADD;
        blend_desc.RenderTarget[0].SrcBlendAlpha = D3D11_BLEND_SRC_ALPHA;
        blend_desc.RenderTarget[0].DestBlendAlpha = D3D11_BLEND_INV_SRC_ALPHA;
        blend_desc.AlphaToCoverageEnable = FALSE;
        device->CreateBlendState(&blend_desc, &no_render_target_write_blend_state);

        blend_desc.RenderTarget[0].BlendEnable = TRUE;
        blend_desc.RenderTarget[0].RenderTargetWriteMask = D3D11_COLOR_WRITE_ENABLE_ALL;
        device->CreateBlendState(&blend_desc, &quad_blend_enabled);

        blend_desc.RenderTarget[0].BlendEnable = FALSE;
        device->CreateBlendState(&blend_desc, &opaque_mesh_blend_disabled);
    }

    //
    // Create sampler states
    //
    {
        D3D11_SAMPLER_DESC sampler_desc = {};
        sampler_desc.Filter         = D3D11_FILTER_MIN_MAG_MIP_POINT;
        sampler_desc.AddressU       = D3D11_TEXTURE_ADDRESS_WRAP;
        sampler_desc.AddressV       = D3D11_TEXTURE_ADDRESS_WRAP;
        sampler_desc.AddressW       = D3D11_TEXTURE_ADDRESS_WRAP;
        sampler_desc.MinLOD         = -FLT_MAX;
        sampler_desc.MaxLOD         = +FLT_MAX;
        sampler_desc.MipLODBias     = 0.0f;
        sampler_desc.MaxAnisotropy  = 1;
        sampler_desc.ComparisonFunc = D3D11_COMPARISON_NEVER;
        sampler_desc.BorderColor[0] = 1;
        sampler_desc.BorderColor[1] = 1;
        sampler_desc.BorderColor[2] = 1;
        sampler_desc.BorderColor[3] = 1;
        device->CreateSamplerState(&sampler_desc, &sampler_point);

        sampler_desc.Filter         = D3D11_FILTER_MIN_MAG_MIP_LINEAR;
        device->CreateSamplerState(&sampler_desc, &sampler_linear);

        sampler_desc.AddressU = D3D11_TEXTURE_ADDRESS_CLAMP;
        sampler_desc.AddressV = D3D11_TEXTURE_ADDRESS_CLAMP;
        sampler_desc.AddressW = D3D11_TEXTURE_ADDRESS_CLAMP;
        device->CreateSamplerState(&sampler_desc, &sampler_linear_clamp);

        sampler_desc.Filter         = D3D11_FILTER_MIN_MAG_MIP_POINT;
        device->CreateSamplerState(&sampler_desc, &sampler_point_clamp);
    }
    
    //
    // Create rasterizer states
    //
    {
        D3D11_RASTERIZER_DESC rasterizer_desc = {};
        rasterizer_desc.FillMode = D3D11_FILL_SOLID;
        rasterizer_desc.CullMode = D3D11_CULL_BACK;
        rasterizer_desc.FrontCounterClockwise = TRUE;
        rasterizer_desc.DepthClipEnable       = TRUE;
        rasterizer_desc.ScissorEnable         = FALSE;
        device->CreateRasterizerState(&rasterizer_desc, &rasterizer_state_for_mesh_rendering);
        Assert(rasterizer_state_for_mesh_rendering);

        rasterizer_desc.CullMode = D3D11_CULL_NONE;
        device->CreateRasterizerState(&rasterizer_desc, &quad_rasterizer_state);

        rasterizer_desc.DepthClipEnable = FALSE;
        rasterizer_desc.CullMode = D3D11_CULL_NONE;
        device->CreateRasterizerState(&rasterizer_desc, &rasterizer_state_for_shadow_rendering);
    }

    //
    // Create depth stencil states
    //
    {
        D3D11_DEPTH_STENCIL_DESC depth_stencil_desc = {};
        depth_stencil_desc.DepthEnable    = TRUE;
        depth_stencil_desc.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ALL;
        depth_stencil_desc.DepthFunc      = D3D11_COMPARISON_LESS_EQUAL;
        device->CreateDepthStencilState(&depth_stencil_desc, &depth_stencil_state_for_mesh_rendering);
        Assert(depth_stencil_state_for_mesh_rendering);

        depth_stencil_desc.DepthEnable    = FALSE;
        depth_stencil_desc.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ZERO;
        device->CreateDepthStencilState(&depth_stencil_desc, &quad_depth_stencil_state);
        Assert(quad_depth_stencil_state);
    }
}

void shutdown_renderer() {
    SafeRelease(depth_stencil_state_for_mesh_rendering);
    SafeRelease(rasterizer_state_for_mesh_rendering);
    SafeRelease(mesh_vertex_input_layout);
    
    release_gpu_buffer(&fullscreen_quad_vb);
    release_gpu_buffer(&fullscreen_quad_ib);
    SafeRelease(quad_input_layout);
    SafeRelease(quad_rasterizer_state);
    SafeRelease(quad_depth_stencil_state);
    
    SafeRelease(sampler_point);
    SafeRelease(sampler_linear);
    
    release_back_buffer();
    
    SafeRelease(swap_chain);
    SafeRelease(device_context);    
    SafeRelease(device);
}

void resize_renderer() {
    release_back_buffer();
    swap_chain->ResizeBuffers(0, 0, 0, DXGI_FORMAT_UNKNOWN, swap_chain_flags);
    init_back_buffer();
}

void render_frame(int num_command_buffers, Command_Buffer *cbs) {
    ZoneScopedN("Render frame internal");
    
    for (int i = 0; i < num_command_buffers; i++) {
        if (cbs[i].context == device_context) continue;
        
        ID3D11CommandList *command_list;
        cbs[i].context->FinishCommandList(TRUE, &command_list);
        device_context->ExecuteCommandList(command_list, FALSE);
        SafeRelease(command_list);
    }
}

void swap_buffers() {
    if (should_vsync) {
        swap_chain->Present(1, 0);
    } else {
        swap_chain->Present(0, DXGI_PRESENT_ALLOW_TEARING);
    }
}

static u32 d3d11_bind_flags(Gpu_Buffer_Type type) {
    switch (type) {
        case GPU_BUFFER_TYPE_VERTEX:   return D3D11_BIND_VERTEX_BUFFER;
        case GPU_BUFFER_TYPE_INDEX:    return D3D11_BIND_INDEX_BUFFER;
        case GPU_BUFFER_TYPE_CONSTANT: return D3D11_BIND_CONSTANT_BUFFER;
    }

    Assert(!"Invalid gpu buffer type");
    return 0;
}

bool create_gpu_buffer(Gpu_Buffer *buffer, Gpu_Buffer_Type type, u32 size, u32 stride, void *initial_data, bool is_dynamic) {
    D3D11_BUFFER_DESC buffer_desc = {};
    buffer_desc.ByteWidth      = size;
    buffer_desc.Usage          = is_dynamic ? D3D11_USAGE_DYNAMIC : D3D11_USAGE_IMMUTABLE;
    buffer_desc.BindFlags      = d3d11_bind_flags(type);
    buffer_desc.CPUAccessFlags = is_dynamic ? D3D11_CPU_ACCESS_WRITE : 0;

    D3D11_SUBRESOURCE_DATA srd = {};
    srd.pSysMem = initial_data;

    if (device->CreateBuffer(&buffer_desc, initial_data ? &srd : NULL, &buffer->buffer) != S_OK) {
        logprintf("Failed to create d3d11 buffer\n");
        return false;
    }

    buffer->type       = type;
    buffer->size       = size;
    buffer->stride     = stride;
    buffer->is_dynamic = is_dynamic;
    
    return true;
}

void release_gpu_buffer(Gpu_Buffer *buffer) {
    SafeRelease(buffer->buffer);
}

static DXGI_FORMAT dxgi_format(Texture_Format format) {
    switch (format) {
        case TEXTURE_FORMAT_RGBA8: return DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;

        case TEXTURE_FORMAT_RGBA16F: return DXGI_FORMAT_R16G16B16A16_FLOAT;

        case TEXTURE_FORMAT_D32:   return DXGI_FORMAT_D32_FLOAT;
    }

    Assert(!"Invalid texture format");
    return DXGI_FORMAT_UNKNOWN;
}

static int get_bytes_per_pixel(Texture_Format format) {
    switch (format) {
        case TEXTURE_FORMAT_RGBA8: return 4;

        case TEXTURE_FORMAT_RGBA16F: return 4 * sizeof(float) / 2;
            
        case TEXTURE_FORMAT_D32:   return 4;
    }

    Assert(!"Invalid texture format");
    return 0;
}

bool create_texture(Texture *texture, int width, int height, Texture_Format format, void *initial_data) {
    int bytes_per_pixel = get_bytes_per_pixel(format);
    
    D3D11_TEXTURE2D_DESC texture_desc = {};
    texture_desc.Width              = width;
    texture_desc.Height             = height;
    texture_desc.MipLevels          = 1;
    texture_desc.ArraySize          = 1;
    texture_desc.Format             = dxgi_format(format);
    texture_desc.SampleDesc.Count   = 1;
    texture_desc.SampleDesc.Quality = 0;
    texture_desc.Usage              = D3D11_USAGE_DEFAULT;
    texture_desc.BindFlags          = D3D11_BIND_SHADER_RESOURCE;

    D3D11_SUBRESOURCE_DATA srd = {};
    srd.pSysMem     = initial_data;
    srd.SysMemPitch = width * bytes_per_pixel;

    if (device->CreateTexture2D(&texture_desc, initial_data ? &srd : NULL, &texture->texture) != S_OK) {
        logprintf("Failed to create d3d11 texture\n");
        return false;
    }

    D3D11_SHADER_RESOURCE_VIEW_DESC srv_desc = {};
    srv_desc.Format                    = texture_desc.Format;
    srv_desc.ViewDimension             = D3D11_SRV_DIMENSION_TEXTURE2D;
    srv_desc.Texture2D.MostDetailedMip = 0;
    srv_desc.Texture2D.MipLevels       = 1;

    if (device->CreateShaderResourceView(texture->texture, &srv_desc, &texture->srv) != S_OK) {
        logprintf("Failed to create d3d11 shader resource view\n");
        return false;
    }

    texture->width           = width;
    texture->height          = height;
    texture->format          = format;
    texture->bytes_per_pixel = bytes_per_pixel;
    
    return true;
}

void release_texture(Texture *texture) {
    SafeRelease(texture->rtv);
    SafeRelease(texture->dsv);
    SafeRelease(texture->srv);
    SafeRelease(texture->texture);
}

static char *read_entire_binary_file(const char *filepath, s64 *length_pointer) {
    FILE *file = fopen(filepath, "rb");
    if (!file) {
        logprintf("Failed to read file '%s'\n", filepath);
        return NULL;
    }
    defer { fclose(file); };

    fseek(file, 0, SEEK_END);
    auto length = ftell(file);
    fseek(file, 0, SEEK_SET);

    char *result = new char[length];
    auto num_read = fread(result, 1, length, file);
    if (length_pointer) *length_pointer = num_read;

    return result;
}

bool load_shader(Shader *shader, String _filename, Render_Vertex_Type vertex_type) {
    const char *filename = temp_c_string(_filename);

    char vertex_full_path[256];
    snprintf(vertex_full_path, sizeof(vertex_full_path), "%s/%s.vertex.fxc", SHADER_DIRECTORY, filename);

    s64 vertex_data_size;
    char *vertex_data = read_entire_binary_file(vertex_full_path, &vertex_data_size);
    if (!vertex_data) return false;
    defer { delete [] vertex_data; };

    if (device->CreateVertexShader(vertex_data, vertex_data_size, NULL, &shader->vertex_shader) != S_OK) {
        logprintf("Failed to create '%s' vertex shader\n", vertex_full_path);
        return false;
    }
    
    char pixel_full_path[256];
    snprintf(pixel_full_path, sizeof(pixel_full_path), "%s/%s.pixel.fxc", SHADER_DIRECTORY, filename);

    s64 pixel_data_size;
    char *pixel_data = read_entire_binary_file(pixel_full_path, &pixel_data_size);
    if (!pixel_data) return false;
    defer { delete [] pixel_data; };
    
    if (device->CreatePixelShader(pixel_data, pixel_data_size, NULL, &shader->pixel_shader) != S_OK) {
        logprintf("Failed to create '%s' pixel shader\n", pixel_full_path);
        return false;
    }

    switch (vertex_type) {
        case RENDER_VERTEX_TYPE_MESH: {
            if (!mesh_vertex_input_layout) {
                D3D11_INPUT_ELEMENT_DESC ieds[6] = {};
                
                ieds[0].SemanticName      = "POSITION";
                ieds[0].Format            = DXGI_FORMAT_R32G32B32_FLOAT;
                ieds[0].AlignedByteOffset = offsetof(Mesh_Vertex, position);
                ieds[0].InputSlotClass    = D3D11_INPUT_PER_VERTEX_DATA;

                ieds[1].SemanticName      = "COLOR";
                ieds[1].Format            = DXGI_FORMAT_R32G32B32A32_FLOAT;
                ieds[1].AlignedByteOffset = offsetof(Mesh_Vertex, color);
                ieds[1].InputSlotClass    = D3D11_INPUT_PER_VERTEX_DATA;

                ieds[2].SemanticName      = "TEXCOORD";
                ieds[2].Format            = DXGI_FORMAT_R32G32_FLOAT;
                ieds[2].AlignedByteOffset = offsetof(Mesh_Vertex, uv);
                ieds[2].InputSlotClass    = D3D11_INPUT_PER_VERTEX_DATA;

                ieds[3].SemanticName      = "NORMAL";
                ieds[3].Format            = DXGI_FORMAT_R32G32B32_FLOAT;
                ieds[3].AlignedByteOffset = offsetof(Mesh_Vertex, normal);
                ieds[3].InputSlotClass    = D3D11_INPUT_PER_VERTEX_DATA;

                ieds[4].SemanticName      = "TANGENT";
                ieds[4].Format            = DXGI_FORMAT_R32G32B32_FLOAT;
                ieds[4].AlignedByteOffset = offsetof(Mesh_Vertex, tangent);
                ieds[4].InputSlotClass    = D3D11_INPUT_PER_VERTEX_DATA;

                ieds[5].SemanticName      = "BITANGENT";
                ieds[5].Format            = DXGI_FORMAT_R32G32B32_FLOAT;
                ieds[5].AlignedByteOffset = offsetof(Mesh_Vertex, bitangent);
                ieds[5].InputSlotClass    = D3D11_INPUT_PER_VERTEX_DATA;

                if (device->CreateInputLayout(ieds, ArrayCount(ieds), vertex_data, vertex_data_size, &mesh_vertex_input_layout) != S_OK) {
                    logprintf("Failed to create mesh vertex input layout with shader '%s'\n", vertex_full_path);
                    return false;
                }
            }

            shader->input_layout = mesh_vertex_input_layout;
        } break;

        case RENDER_VERTEX_TYPE_QUAD: {
            if (!quad_input_layout) {
                D3D11_INPUT_ELEMENT_DESC ieds[3] = {};
                
                ieds[0].SemanticName      = "POSITION";
                ieds[0].Format            = DXGI_FORMAT_R32G32_FLOAT;
                ieds[0].AlignedByteOffset = offsetof(Quad_Vertex, position);
                ieds[0].InputSlotClass    = D3D11_INPUT_PER_VERTEX_DATA;

                ieds[1].SemanticName      = "COLOR";
                ieds[1].Format            = DXGI_FORMAT_R32G32B32A32_FLOAT;
                ieds[1].AlignedByteOffset = offsetof(Quad_Vertex, color);
                ieds[1].InputSlotClass    = D3D11_INPUT_PER_VERTEX_DATA;
                
                ieds[2].SemanticName      = "TEXCOORD";
                ieds[2].Format            = DXGI_FORMAT_R32G32_FLOAT;
                ieds[2].AlignedByteOffset = offsetof(Quad_Vertex, uv);
                ieds[2].InputSlotClass    = D3D11_INPUT_PER_VERTEX_DATA;

                if (device->CreateInputLayout(ieds, ArrayCount(ieds), vertex_data, vertex_data_size, &quad_input_layout) != S_OK) {
                    logprintf("Failed to create quad vertex input layout with shader '%s'\n", vertex_full_path);
                    return false;
                }
            }

            shader->input_layout = quad_input_layout;
        } break;
    }

    return true;
}

void release_shader(Shader *shader) {
    SafeRelease(shader->pixel_shader);
    SafeRelease(shader->vertex_shader);
}

void imgui_init_dx11() {
    ImGui_ImplDX11_Init(device, device_context);
}

void imgui_begin_frame_dx11() {
     ImGui_ImplDX11_NewFrame();
}

void imgui_end_frame_dx11() {
    ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());
}

void imgui_shutdown_dx11() {
    ImGui_ImplDX11_Shutdown();
}

bool init_command_buffer(Command_Buffer *cb) {
    device->CreateDeferredContext(0, &cb->context);
    
    if (!create_gpu_buffer(&cb->per_scene_cb,     GPU_BUFFER_TYPE_CONSTANT, sizeof(Per_Scene_Uniforms), 0, NULL, true)) return false;
    if (!create_gpu_buffer(&cb->per_object_cb,    GPU_BUFFER_TYPE_CONSTANT, sizeof(Per_Object_Uniforms), 0, NULL, true)) return false;
    if (!create_gpu_buffer(&cb->per_subobject_cb, GPU_BUFFER_TYPE_CONSTANT, sizeof(Per_Subobject_Uniforms), 0, NULL, true)) return false;
    
    return true;
}

void release_command_buffer(Command_Buffer *cb) {
    release_gpu_buffer(&cb->per_subobject_cb);
    release_gpu_buffer(&cb->per_object_cb);
    release_gpu_buffer(&cb->per_scene_cb);

    SafeRelease(cb->context);
}

void set_render_targets(Command_Buffer *cb, int num_render_targets, Texture *render_targets, Texture *depth_target) {
    Assert(cb);
    
    const int MAX_RENDER_TARGETS = 4;
    Assert(num_render_targets < MAX_RENDER_TARGETS);
    
    ID3D11RenderTargetView *rtvs[MAX_RENDER_TARGETS];
    for (int i = 0; i < num_render_targets; i++) {
        rtvs[i] = render_targets[i].rtv;
    }
    
    cb->context->OMSetRenderTargets(num_render_targets, num_render_targets > 0 ? rtvs : NULL, depth_target ? depth_target->dsv : NULL);
}

void clear_render_target(Command_Buffer *cb, Texture *render_target, Vector4 clear_color) {
    Assert(cb);
    Assert(render_target);

    cb->context->ClearRenderTargetView(render_target->rtv, &clear_color.x);
}

void set_viewport(Command_Buffer *cb, int width, int height) {
    Assert(cb);
    
    D3D11_VIEWPORT viewport = {};
    viewport.Width    = (float)width;
    viewport.Height   = (float)height;
    viewport.MaxDepth = 1.0f;
    cb->context->RSSetViewports(1, &viewport);
}

void clear_depth_target(Command_Buffer *cb, Texture *depth_target, float z, u8 stencil) {
    Assert(cb);
    Assert(depth_target);

    // TODO: Store whether the depth_target has a format with stencil and set the clear flags accordingly.
    UINT clear_flags = D3D11_CLEAR_DEPTH;
    cb->context->ClearDepthStencilView(depth_target->dsv, clear_flags, 1.0f, 0);
}

void set_pipeline_type(Command_Buffer *cb, Render_Pipeline_Type type, Shader *shader) {
    switch (type) {
        case RENDER_PIPELINE_MESH: {
            cb->context->RSSetState(rasterizer_state_for_mesh_rendering);
            cb->context->OMSetDepthStencilState(depth_stencil_state_for_mesh_rendering, 0);
            cb->context->IASetInputLayout(mesh_vertex_input_layout);
            cb->context->OMSetBlendState(opaque_mesh_blend_disabled, NULL, 0xFFFFFFFF);

            for (int i = 0; i < MAX_SHADOW_CASCADES; i++) {
                set_texture(cb, (Texture_Type)(TEXTURE_SHADOW_MAP + i), &shadow_map_targets[i]);
            }
        } break;

        case RENDER_PIPELINE_QUAD: {
            cb->context->RSSetState(quad_rasterizer_state);
            cb->context->OMSetDepthStencilState(quad_depth_stencil_state, 0);
            cb->context->IASetInputLayout(quad_input_layout);
            //cb->context->OMSetBlendState(quad_blend_enabled, NULL, 0xFFFFFFFF);
            cb->context->OMSetBlendState(opaque_mesh_blend_disabled, NULL, 0xFFFFFFFF);
        } break;

        case RENDER_PIPELINE_SHADOW: {
            cb->context->RSSetState(rasterizer_state_for_shadow_rendering);
            cb->context->OMSetDepthStencilState(depth_stencil_state_for_mesh_rendering, 0);
            cb->context->OMSetBlendState(no_render_target_write_blend_state, NULL, 0xFFFFFFFF);
            cb->context->IASetInputLayout(mesh_vertex_input_layout);
        } break;
    }

    Assert(shader);
    cb->context->VSSetShader(shader->vertex_shader, NULL, 0);
    cb->context->PSSetShader(shader->pixel_shader, NULL, 0);
    
    ID3D11Buffer *cbs[] = { cb->per_scene_cb.buffer, cb->per_object_cb.buffer, cb->per_subobject_cb.buffer };
    cb->context->VSSetConstantBuffers(0, ArrayCount(cbs), cbs);
    cb->context->PSSetConstantBuffers(0, ArrayCount(cbs), cbs);
    
    ID3D11SamplerState *samplers[] = { sampler_point, sampler_linear, sampler_point_clamp, sampler_linear_clamp };
    cb->context->PSSetSamplers(0, ArrayCount(samplers), samplers);

    cb->context->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
}

void set_per_scene_uniforms(Command_Buffer *cb, Per_Scene_Uniforms *uniforms) {
    D3D11_MAPPED_SUBRESOURCE msr;
    cb->context->Map(cb->per_scene_cb.buffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &msr);
    memcpy(msr.pData, uniforms, sizeof(*uniforms));
    cb->context->Unmap(cb->per_scene_cb.buffer, 0);
}

void set_per_object_uniforms(Command_Buffer *cb, Per_Object_Uniforms *uniforms) {
    D3D11_MAPPED_SUBRESOURCE msr;
    cb->context->Map(cb->per_object_cb.buffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &msr);
    memcpy(msr.pData, uniforms, sizeof(*uniforms));
    cb->context->Unmap(cb->per_object_cb.buffer, 0);
}

void render_item(Command_Buffer *cb, Render_Item_Info *info) {
    Assert(info->vertex_buffer);
    Assert(info->index_buffer);
    Assert(info->albedo_texture);
    
    UINT offset = 0;
    cb->context->IASetVertexBuffers(0, 1, &info->vertex_buffer->buffer, &info->vertex_buffer->stride, &offset);
    cb->context->IASetIndexBuffer(info->index_buffer->buffer, DXGI_FORMAT_R32_UINT, 0);

    set_texture(cb, TEXTURE_ALBEDO, info->albedo_texture);
    set_texture(cb, TEXTURE_NORMAL, info->normal_texture);
    set_texture(cb, TEXTURE_METALLIC_ROUGHNESS, info->metallic_roughness_texture);
    set_texture(cb, TEXTURE_AO,                 info->ao_texture);
    set_texture(cb, TEXTURE_EMISSIVE,           info->emissive_texture);

    D3D11_MAPPED_SUBRESOURCE msr;
    cb->context->Map(cb->per_subobject_cb.buffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &msr);
    memcpy(msr.pData, &info->uniforms, sizeof(info->uniforms));
    cb->context->Unmap(cb->per_subobject_cb.buffer, 0);
    
    cb->context->DrawIndexed(info->num_indices, 0, 0);
}

void set_texture(Command_Buffer *cb, Texture_Type type, Texture *texture) {
    if (texture) {
        cb->context->PSSetShaderResources((int)type, 1, &texture->srv);
    } else {
        ID3D11ShaderResourceView *null_srv[1] = { NULL };
        cb->context->PSSetShaderResources((int)type, 1, null_srv);
    }
}

Memory_Budget get_vram_memory() {
    IDXGIDevice *dxgi_device = NULL;
    device->QueryInterface(IID_PPV_ARGS(&dxgi_device));
    defer { SafeRelease(dxgi_device); };

    IDXGIAdapter *dxgi_adapter = NULL;
    dxgi_device->GetAdapter(&dxgi_adapter);
    defer { SafeRelease(dxgi_adapter); };

    IDXGIAdapter3 *adapter = NULL;
    dxgi_adapter->QueryInterface(IID_PPV_ARGS(&adapter));
    defer { SafeRelease(adapter); };

    DXGI_QUERY_VIDEO_MEMORY_INFO memory_info = {};
    adapter->QueryVideoMemoryInfo(0, DXGI_MEMORY_SEGMENT_GROUP_LOCAL, &memory_info);

    Memory_Budget result = {};

    result.used = memory_info.CurrentUsage;
    result.max  = memory_info.Budget;

    return result;
}
