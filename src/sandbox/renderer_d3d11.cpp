#include "pch.h"
#include "renderer.h"
#include "mesh.h"
#include "renderer_internal.h"

#include <d3d11.h>
#include <stdio.h>
#include <stdlib.h>
#include <float.h>

#include <imgui.h>

#ifdef RENDER_D3D11
#include <imgui_impl_dx11.h>
#endif

#define SafeRelease(ptr) if (ptr) { ptr->Release(); ptr = NULL; }

static bool should_vsync;

extern Shader shader_basic;
extern Shader shader_resolve;

extern bool init_shaders();

static ID3D11Device *device;
static ID3D11DeviceContext *device_context;
static IDXGISwapChain *swap_chain;
static u32 swap_chain_flags;

static ID3D11Texture2D *back_buffer_texture = NULL;
static ID3D11RenderTargetView *back_buffer_rtv = NULL;

static ID3D11Texture2D *offscreen_buffer_texture = NULL;
static ID3D11RenderTargetView *offscreen_buffer_rtv = NULL;
static ID3D11ShaderResourceView *offscreen_buffer_srv = NULL;

static ID3D11Texture2D *depth_buffer_texture = NULL;
static ID3D11DepthStencilView *depth_buffer_dsv = NULL;

static ID3D11SamplerState *sampler_point  = NULL;
static ID3D11SamplerState *sampler_linear = NULL;

static Gpu_Buffer per_scene_cb;
static Gpu_Buffer per_object_cb;

static Gpu_Buffer fullscreen_quad_vb;
static Gpu_Buffer fullscreen_quad_ib;
static ID3D11InputLayout *quad_input_layout = NULL;
static ID3D11RasterizerState *quad_rasterizer_state = NULL;
static ID3D11DepthStencilState *quad_depth_stencil_state = NULL;

static ID3D11InputLayout *mesh_vertex_input_layout = NULL;
static ID3D11RasterizerState *rasterizer_state_for_mesh_rendering = NULL;
static ID3D11DepthStencilState *depth_stencil_state_for_mesh_rendering = NULL;

static void init_back_buffer() {
    swap_chain->GetBuffer(0, IID_PPV_ARGS(&back_buffer_texture));

    D3D11_RENDER_TARGET_VIEW_DESC rtv_desc = {};
    rtv_desc.Format        = DXGI_FORMAT_B8G8R8A8_UNORM_SRGB;
    rtv_desc.ViewDimension = D3D11_RTV_DIMENSION_TEXTURE2D;
    device->CreateRenderTargetView(back_buffer_texture, &rtv_desc, &back_buffer_rtv);

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
    device->CreateTexture2D(&texture_desc, NULL, &depth_buffer_texture);

    D3D11_DEPTH_STENCIL_VIEW_DESC dsv_desc = {};
    dsv_desc.Format        = texture_desc.Format;
    dsv_desc.ViewDimension = D3D11_DSV_DIMENSION_TEXTURE2D;
    device->CreateDepthStencilView(depth_buffer_texture, &dsv_desc, &depth_buffer_dsv);

    texture_desc.Format    = DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;
    texture_desc.BindFlags = D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE;
    device->CreateTexture2D(&texture_desc, NULL, &offscreen_buffer_texture);

    rtv_desc.Format = DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;
    device->CreateRenderTargetView(offscreen_buffer_texture, &rtv_desc, &offscreen_buffer_rtv);

    D3D11_SHADER_RESOURCE_VIEW_DESC srv_desc = {};
    srv_desc.Format                    = texture_desc.Format;
    srv_desc.ViewDimension             = D3D11_SRV_DIMENSION_TEXTURE2D;
    srv_desc.Texture2D.MostDetailedMip = 0;
    srv_desc.Texture2D.MipLevels       = 1;
    device->CreateShaderResourceView(offscreen_buffer_texture, &srv_desc, &offscreen_buffer_srv);
}

static void release_back_buffer() {
    SafeRelease(depth_buffer_dsv);
    SafeRelease(depth_buffer_texture);

    SafeRelease(offscreen_buffer_srv);
    SafeRelease(offscreen_buffer_rtv);
    SafeRelease(offscreen_buffer_texture);
    
    SafeRelease(back_buffer_rtv);
    SafeRelease(back_buffer_texture);
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

    D3D_FEATURE_LEVEL feature_levels[] = { D3D_FEATURE_LEVEL_11_0 };

    UINT create_flags = D3D11_CREATE_DEVICE_BGRA_SUPPORT | D3D11_CREATE_DEVICE_SINGLETHREADED;
#ifdef BUILD_DEBUG
    create_flags |= D3D11_CREATE_DEVICE_DEBUG;
#endif
    
    D3D11CreateDeviceAndSwapChain(NULL, D3D_DRIVER_TYPE_HARDWARE, NULL, create_flags, feature_levels, ArrayCount(feature_levels), D3D11_SDK_VERSION, &swap_chain_desc, &swap_chain, &device, NULL, &device_context);

    Assert(swap_chain);
    Assert(device);
    Assert(device_context);

    init_back_buffer();

    if (!init_shaders()) {
        exit(1);
    }

    render_commands     = new Render_Command[MAX_RENDER_COMMANDS];
    num_render_commands = 0;

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
    }

    //
    // Create constant buffers
    //
    {
        create_gpu_buffer(&per_scene_cb, GPU_BUFFER_TYPE_CONSTANT, sizeof(Per_Scene_Uniforms), 0, NULL, true);
        create_gpu_buffer(&per_object_cb, GPU_BUFFER_TYPE_CONSTANT, sizeof(Per_Object_Uniforms), 0, NULL, true);
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

void resize_renderer() {
    release_back_buffer();
    swap_chain->ResizeBuffers(0, 0, 0, DXGI_FORMAT_UNKNOWN, swap_chain_flags);
    init_back_buffer();
}

void render_frame(Vector4 clear_color) {
    ZoneScopedN("Render frame internal");
    
    device_context->ClearRenderTargetView(offscreen_buffer_rtv, &clear_color.x);
    device_context->ClearDepthStencilView(depth_buffer_dsv, D3D11_CLEAR_DEPTH, 1.0f, 0);
    device_context->OMSetRenderTargets(1, &offscreen_buffer_rtv, depth_buffer_dsv);

    D3D11_VIEWPORT viewport = {};
    viewport.Width    = (float)platform_window_width;
    viewport.Height   = (float)platform_window_height;
    viewport.MaxDepth = 1.0f;
    device_context->RSSetViewports(1, &viewport);

    ID3D11SamplerState *samplers[] = { sampler_point, sampler_linear };
    device_context->PSSetSamplers(0, ArrayCount(samplers), samplers);

    ID3D11Buffer *cbs[] = { per_scene_cb.buffer, per_object_cb.buffer };
    device_context->VSSetConstantBuffers(0, ArrayCount(cbs), cbs);
    device_context->PSSetConstantBuffers(0, ArrayCount(cbs), cbs);

    device_context->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

    // TODO: Change this to an enum
    int render_pass = -1; // 0 - mesh rendering

    for (int i = 0; i < num_render_commands; i++) {
        Render_Command *command = &render_commands[i];
        
        switch (command->type) {
            case RENDER_COMMAND_SET_PER_SCENE_UNIFORMS: {
                Per_Scene_Uniforms *uniforms = &command->per_scene_uniforms;

                D3D11_MAPPED_SUBRESOURCE msr;
                device_context->Map(per_scene_cb.buffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &msr);
                memcpy(msr.pData, uniforms, sizeof(*uniforms));
                device_context->Unmap(per_scene_cb.buffer, 0);
            } break;

            case RENDER_COMMAND_RENDER_MESH: {
                if (render_pass != 0) {
                    device_context->RSSetState(rasterizer_state_for_mesh_rendering);
                    device_context->OMSetDepthStencilState(depth_stencil_state_for_mesh_rendering, 0);
                    device_context->VSSetShader(shader_basic.vertex_shader, NULL, 0);
                    device_context->PSSetShader(shader_basic.pixel_shader,  NULL, 0);
                    device_context->IASetInputLayout(mesh_vertex_input_layout);
                    
                    render_pass = 0;
                }
                
                Render_Mesh_Info *info = &command->render_mesh_info;

                D3D11_MAPPED_SUBRESOURCE msr;
                device_context->Map(per_object_cb.buffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &msr);
                memcpy(msr.pData, &info->uniforms, sizeof(Per_Object_Uniforms));
                device_context->Unmap(per_object_cb.buffer, 0);
                
                Assert(info->vertex_buffer);
                ID3D11Buffer *vertex_buffer = info->vertex_buffer->buffer;
                UINT offset = 0;
                device_context->IASetVertexBuffers(0, 1, &vertex_buffer, &info->vertex_buffer->stride, &offset);

                Assert(info->index_buffer);
                ID3D11Buffer *index_buffer = info->index_buffer->buffer;
                device_context->IASetIndexBuffer(index_buffer, DXGI_FORMAT_R32_UINT, 0);

                Assert(info->diffuse_texture);
                device_context->PSSetShaderResources(0, 1, &info->diffuse_texture->srv);

                device_context->DrawIndexed(info->num_indices, 0, 0);
            } break;
        }
    }

    float resolve_clear_color[4] = { 0.0f, 0.0f, 0.0f, 1.0f };
    device_context->ClearRenderTargetView(back_buffer_rtv, resolve_clear_color);
    device_context->OMSetRenderTargets(1, &back_buffer_rtv, NULL);

    device_context->RSSetState(quad_rasterizer_state);
    device_context->OMSetDepthStencilState(quad_depth_stencil_state, 0);
    device_context->VSSetShader(shader_resolve.vertex_shader, NULL, 0);
    device_context->PSSetShader(shader_resolve.pixel_shader,  NULL, 0);
    device_context->IASetInputLayout(quad_input_layout);
    UINT offset = 0;
    device_context->IASetVertexBuffers(0, 1, &fullscreen_quad_vb.buffer, &fullscreen_quad_vb.stride, &offset);
    device_context->IASetIndexBuffer(fullscreen_quad_ib.buffer, DXGI_FORMAT_R32_UINT, 0);
    device_context->PSSetShaderResources(0, 1, &offscreen_buffer_srv);
    device_context->DrawIndexed(6, 0, 0);
    
    num_render_commands = 0;
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

static DXGI_FORMAT dxgi_format(Texture_Format format) {
    switch (format) {
        case TEXTURE_FORMAT_RGBA8: return DXGI_FORMAT_R8G8B8A8_UNORM;

        case TEXTURE_FORMAT_D32:   return DXGI_FORMAT_D32_FLOAT;
    }

    Assert(!"Invalid texture format");
    return DXGI_FORMAT_UNKNOWN;
}

static int get_bytes_per_pixel(Texture_Format format) {
    switch (format) {
        case TEXTURE_FORMAT_RGBA8: return 4;

        case TEXTURE_FORMAT_D32:   return 4;
    }

    Assert(!"Invalid texture format");
    return 0;
}

bool create_texture(Texture *texture, int width, int height, Texture_Format format, u8 *initial_data) {
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

    if (device->CreateVertexShader(vertex_data, vertex_data_size, NULL, &shader->vertex_shader) != S_OK) {
        logprintf("Failed to create '%s' vertex shader\n", vertex_full_path);
        return false;
    }
    
    char pixel_full_path[256];
    snprintf(pixel_full_path, sizeof(pixel_full_path), "%s/%s.pixel.fxc", SHADER_DIRECTORY, filename);

    s64 pixel_data_size;
    char *pixel_data = read_entire_binary_file(pixel_full_path, &pixel_data_size);
    if (!pixel_data) return false;

    if (device->CreatePixelShader(pixel_data, pixel_data_size, NULL, &shader->pixel_shader) != S_OK) {
        logprintf("Failed to create '%s' pixel shader\n", pixel_full_path);
        return false;
    }

    switch (vertex_type) {
        case RENDER_VERTEX_TYPE_MESH: {
            if (!mesh_vertex_input_layout) {
                D3D11_INPUT_ELEMENT_DESC ieds[5] = {};
                
                ieds[0].SemanticName      = "POSITION";
                ieds[0].Format            = DXGI_FORMAT_R32G32B32_FLOAT;
                ieds[0].AlignedByteOffset = offsetof(Mesh_Vertex, position);
                ieds[0].InputSlotClass    = D3D11_INPUT_PER_VERTEX_DATA;

                ieds[1].SemanticName      = "TEXCOORD";
                ieds[1].Format            = DXGI_FORMAT_R32G32_FLOAT;
                ieds[1].AlignedByteOffset = offsetof(Mesh_Vertex, uv);
                ieds[1].InputSlotClass    = D3D11_INPUT_PER_VERTEX_DATA;

                ieds[2].SemanticName      = "NORMAL";
                ieds[2].Format            = DXGI_FORMAT_R32G32B32_FLOAT;
                ieds[2].AlignedByteOffset = offsetof(Mesh_Vertex, normal);
                ieds[2].InputSlotClass    = D3D11_INPUT_PER_VERTEX_DATA;

                ieds[3].SemanticName      = "TANGENT";
                ieds[3].Format            = DXGI_FORMAT_R32G32B32_FLOAT;
                ieds[3].AlignedByteOffset = offsetof(Mesh_Vertex, tangent);
                ieds[3].InputSlotClass    = D3D11_INPUT_PER_VERTEX_DATA;

                ieds[4].SemanticName      = "BITANGENT";
                ieds[4].Format            = DXGI_FORMAT_R32G32B32_FLOAT;
                ieds[4].AlignedByteOffset = offsetof(Mesh_Vertex, bitangent);
                ieds[4].InputSlotClass    = D3D11_INPUT_PER_VERTEX_DATA;

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

void imgui_init_dx11() {
    ImGui_ImplDX11_Init(device, device_context);
}

void imgui_begin_frame_dx11() {
     ImGui_ImplDX11_NewFrame();
}

void imgui_end_frame_dx11() {
    ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());
}
