#pragma once

#include "renderer.h"

#include <d3d12.h>
#include <dxgi1_6.h>
#include <d3dcompiler.h>

#define SafeRelease(ptr) if (ptr) { ptr->Release(); ptr = NULL; }

#ifdef BUILD_DEBUG

#define AssertHR(hr) if (hr != ERROR_SUCCESS) { print_hr(hr); Assert(false); } else {}

inline void print_hr(HRESULT hr) {
    wchar_t buf[4096] = {};
    DWORD num_chars = FormatMessageW(FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS, NULL, hr, 0, buf, ArrayCount(buf), NULL);
    logprintf("Error value: %d Message %ws\n", hr, num_chars ? buf : L"Error message not found");
}

#else

#define AssertHR(hr)

#endif

struct Shader_D3D12 : public Shader {
    ID3D12RootSignature *root_signature;
    ID3D12PipelineState *pipeline_state;
};

struct Gpu_Buffer_D3D12 : public Gpu_Buffer {
    ID3D12Resource *resource;
    D3D12_VERTEX_BUFFER_VIEW vertex_buffer_view;
    D3D12_INDEX_BUFFER_VIEW index_buffer_view;
};

struct Texture_D3D12 : public Texture {
    ID3D12Resource *resource;
    D3D12_GPU_DESCRIPTOR_HANDLE descriptor_handle;
};

struct Renderer_D3D12 : public Renderer {
    Memory_Arena gpu_resources_memory;
    int num_allocated_textures;
    
    bool use_warp_device;
    
    IDXGISwapChain3 *swap_chain;
    ID3D12Device *device;
    ID3D12Resource *back_buffers[NUM_FRAMES];
    ID3D12CommandAllocator *command_allocators[NUM_FRAMES];
    ID3D12CommandQueue *command_queue;
    ID3D12DescriptorHeap *rtv_heap;
    ID3D12DescriptorHeap *srv_heap;
    ID3D12GraphicsCommandList *command_list;
    UINT rtv_descriptor_size;
    UINT srv_descriptor_size;

    // Synchronization objects.
    UINT frame_index;
    HANDLE fence_event;
    ID3D12Fence *fence;
    UINT64 fence_values[NUM_FRAMES];
    
    D3D12_VIEWPORT viewport;
    D3D12_RECT scissor_rect;

    ID3D12Resource *per_scene_cb;
    UINT8 *per_scene_cb_data;
    D3D12_GPU_DESCRIPTOR_HANDLE per_scene_cb_descriptor_handle;
};

Renderer *renderer_d3d12_create(Platform_Window *window, bool vsync);
void renderer_d3d12_shutdown(Renderer_D3D12 *renderer);

void renderer_d3d12_execute_render_commands_and_present(Renderer_D3D12 *renderer);
void renderer_d3d12_move_to_next_frame(Renderer_D3D12 *renderer);
void renderer_d3d12_wait_for_gpu(Renderer_D3D12 *renderer);

Shader *renderer_d3d12_load_shader(Renderer_D3D12 *renderer, Shader_Info info);
Gpu_Buffer *renderer_d3d12_allocate_buffer(Renderer_D3D12 *renderer, Gpu_Buffer_Type type, u32 size, u32 stride, void *initial_data);
void renderer_d3d12_update_entire_buffer(Renderer_D3D12 *renderer, Gpu_Buffer_D3D12 *buffer, u32 size, void *data);
Texture *renderer_d3d12_allocate_texture(Renderer_D3D12 *renderer, int width, int height, Texture_Format format, int bpp, void *pixels);
