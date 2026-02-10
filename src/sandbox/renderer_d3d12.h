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

struct Renderer_D3D12 : public Renderer {
    bool use_warp_device;
    
    IDXGISwapChain3 *swap_chain;
    ID3D12Device *device;
    ID3D12Resource *back_buffers[NUM_FRAMES];
    ID3D12CommandAllocator *command_allocators[NUM_FRAMES];
    ID3D12CommandQueue *command_queue;
    ID3D12DescriptorHeap *rtv_heap;
    ID3D12GraphicsCommandList *command_list;
    UINT rtv_descriptor_size;

    // Synchronization objects.
    UINT frame_index;
    HANDLE fence_event;
    ID3D12Fence *fence;
    UINT64 fence_values[NUM_FRAMES];
    
    D3D12_VIEWPORT viewport;
    D3D12_RECT scissor_rect;
    ID3D12RootSignature *root_signature;
    ID3D12PipelineState *pipeline_state;

    ID3D12Resource *vertex_buffer;
    D3D12_VERTEX_BUFFER_VIEW vertex_buffer_view;
};

Renderer *renderer_d3d12_create(Platform_Window *window, bool vsync);
void renderer_d3d12_shutdown(Renderer_D3D12 *renderer);

void renderer_d3d12_execute_render_commands(Renderer_D3D12 *renderer);
void renderer_d3d12_move_to_next_frame(Renderer_D3D12 *renderer);
void renderer_d3d12_wait_for_gpu(Renderer_D3D12 *renderer);
