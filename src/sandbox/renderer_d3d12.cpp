#include "pch.h"
#include "renderer_d3d12.h"

// From https://github.com/microsoft/DirectX-Graphics-Samples/tree/master/Samples/Desktop/D3D12HelloWorld/src

static void get_hardware_adapter(IDXGIFactory1 *factory, IDXGIAdapter1 **adapter_pointer, bool request_high_performance_adapter = true)
{
    *adapter_pointer = NULL;

    IDXGIAdapter1 *adapter = NULL;

    IDXGIFactory6 *factory6 = NULL;
    defer { SafeRelease(factory6); };
    if (SUCCEEDED(factory->QueryInterface(IID_PPV_ARGS(&factory6)))) {
        for (UINT adapter_index = 0; SUCCEEDED(factory6->EnumAdapterByGpuPreference(adapter_index, request_high_performance_adapter == true ? DXGI_GPU_PREFERENCE_HIGH_PERFORMANCE : DXGI_GPU_PREFERENCE_UNSPECIFIED, IID_PPV_ARGS(&adapter))); ++adapter_index) {
            DXGI_ADAPTER_DESC1 desc;
            adapter->GetDesc1(&desc);

            if (desc.Flags & DXGI_ADAPTER_FLAG_SOFTWARE) {
                // Don't select the Basic Render Driver adapter.
                // If you want a software adapter, pass in "/warp" on the command line.
                continue;
            }

            // Check to see whether the adapter supports Direct3D 12, but don't create the
            // actual device yet.
            if (SUCCEEDED(D3D12CreateDevice(adapter, D3D_FEATURE_LEVEL_11_0, _uuidof(ID3D12Device), NULL))) {
                break;
            }
        }
    }

    if (adapter == nullptr) {
        for (UINT adapter_index = 0; SUCCEEDED(factory->EnumAdapters1(adapter_index, &adapter)); ++adapter_index) {
            DXGI_ADAPTER_DESC1 desc;
            adapter->GetDesc1(&desc);

            if (desc.Flags & DXGI_ADAPTER_FLAG_SOFTWARE) {
                // Don't select the Basic Render Driver adapter.
                // If you want a software adapter, pass in "/warp" on the command line.
                continue;
            }

            // Check to see whether the adapter supports Direct3D 12, but don't create the
            // actual device yet.
            if (SUCCEEDED(D3D12CreateDevice(adapter, D3D_FEATURE_LEVEL_11_0, _uuidof(ID3D12Device), nullptr))) {
                break;
            }
        }
    }
    
    *adapter_pointer = adapter;
}

Renderer *renderer_d3d12_create(Platform_Window *window, bool vsync) {
    auto renderer = MaAllocStruct(&globals.permanent_memory, Renderer_D3D12, false);

     UINT dxgi_factory_flags = 0;

#ifdef BUILD_DEBUG
    // Enable the debug layer (requires the Graphics Tools "optional feature").
    // NOTE: Enabling the debug layer after device creation will invalidate the active device.
    {
        ID3D12Debug *debug_controller = NULL;
        defer { SafeRelease(debug_controller); };
        if (SUCCEEDED(D3D12GetDebugInterface(IID_PPV_ARGS(&debug_controller)))) {
            debug_controller->EnableDebugLayer();
            dxgi_factory_flags |= DXGI_CREATE_FACTORY_DEBUG;
        }
    }
#endif

    IDXGIFactory4 *factory = NULL;
    defer { SafeRelease(factory); };
    HRESULT hr = CreateDXGIFactory2(dxgi_factory_flags, IID_PPV_ARGS(&factory));
    AssertHR(hr);

    if (renderer->use_warp_device) {
        IDXGIAdapter *warp_adapter = NULL;
        defer { SafeRelease(warp_adapter); };
        hr = factory->EnumWarpAdapter(IID_PPV_ARGS(&warp_adapter));
        AssertHR(hr);

        hr = D3D12CreateDevice(warp_adapter, D3D_FEATURE_LEVEL_11_0, IID_PPV_ARGS(&renderer->device));
        AssertHR(hr);
    } else {
        IDXGIAdapter1 *hardware_adapter = NULL;
        defer { SafeRelease(hardware_adapter); };
        get_hardware_adapter(factory, &hardware_adapter);

        hr = D3D12CreateDevice(hardware_adapter, D3D_FEATURE_LEVEL_11_0, IID_PPV_ARGS(&renderer->device));
        AssertHR(hr);
    }

    D3D12_COMMAND_QUEUE_DESC queue_desc = {};
    queue_desc.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE;
    queue_desc.Type  = D3D12_COMMAND_LIST_TYPE_DIRECT;

    hr = renderer->device->CreateCommandQueue(&queue_desc, IID_PPV_ARGS(&renderer->command_queue));
    AssertHR(hr);

    DXGI_SWAP_CHAIN_DESC1 swap_chain_desc = {};
    swap_chain_desc.BufferCount      = Renderer::NUM_FRAMES;
    swap_chain_desc.Width            = window->width;
    swap_chain_desc.Height           = window->height;
    swap_chain_desc.Format           = DXGI_FORMAT_R8G8B8A8_UNORM;
    swap_chain_desc.BufferUsage      = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    swap_chain_desc.SwapEffect       = DXGI_SWAP_EFFECT_FLIP_DISCARD;
    swap_chain_desc.SampleDesc.Count = 1;

    IDXGISwapChain1 *swap_chain = NULL;
    defer { SafeRelease(swap_chain); };

    // Swap chain needs the queue so that it can force a flush on it.
    hr = factory->CreateSwapChainForHwnd(renderer->command_queue, (HWND)platform_window_get_native(window), &swap_chain_desc, NULL, NULL, &swap_chain);
    AssertHR(hr);
    
    hr = factory->MakeWindowAssociation((HWND)platform_window_get_native(window), DXGI_MWA_NO_ALT_ENTER);
    AssertHR(hr);

    swap_chain->QueryInterface(IID_PPV_ARGS(&renderer->swap_chain));
    
    renderer->frame_index = renderer->swap_chain->GetCurrentBackBufferIndex();

    // Create descriptor heaps.
    {
        // Describe and create a render target view (RTV) descriptor heap.
        D3D12_DESCRIPTOR_HEAP_DESC rtv_heap_desc = {};
        rtv_heap_desc.NumDescriptors = Renderer::NUM_FRAMES;
        rtv_heap_desc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
        rtv_heap_desc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
        hr = renderer->device->CreateDescriptorHeap(&rtv_heap_desc, IID_PPV_ARGS(&renderer->rtv_heap));
        AssertHR(hr);

        renderer->rtv_descriptor_size = renderer->device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
    }

    // Create frame resources.
    {
        D3D12_CPU_DESCRIPTOR_HANDLE rtv_handle = renderer->rtv_heap->GetCPUDescriptorHandleForHeapStart();

        // Create a RTV for each frame.
        for (UINT n = 0; n < Renderer::NUM_FRAMES; n++)
        {
            HRESULT hr = renderer->swap_chain->GetBuffer(n, IID_PPV_ARGS(&renderer->back_buffers[n]));
            AssertHR(hr);
            
            renderer->device->CreateRenderTargetView(renderer->back_buffers[n], NULL, rtv_handle);
            rtv_handle.ptr += renderer->rtv_descriptor_size;
        }
    }

    hr = renderer->device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&renderer->command_allocator));
    AssertHR(hr);

    // Create the command list.
    hr = renderer->device->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, renderer->command_allocator, NULL, IID_PPV_ARGS(&renderer->command_list));
    AssertHR(hr);

    // Command lists are created in the recording state, but there is nothing
    // to record yet. The main loop expects it to be closed, so close it now.
    hr = renderer->command_list->Close();
    AssertHR(hr);

    // Create synchronization objects.
    {
        hr = renderer->device->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&renderer->fence));
        AssertHR(hr);
        
        renderer->fence_value = 1;

        // Create an event handle to use for frame synchronization.
        renderer->fence_event = CreateEvent(NULL, FALSE, FALSE, NULL);
        if (renderer->fence_event == nullptr) {
            //ThrowIfFailed(HRESULT_FROM_WIN32(GetLastError()));
        }
    }
    
    return renderer;
}

void renderer_d3d12_shutdown(Renderer_D3D12 *renderer) {
    renderer_d3d12_wait_for_previous_frame(renderer);

    CloseHandle(renderer->fence_event);
}

void renderer_d3d12_execute_render_commands(Renderer_D3D12 *renderer) {
    // Command list allocators can only be reset when the associated 
    // command lists have finished execution on the GPU; apps should use 
    // fences to determine GPU execution progress.
    HRESULT hr = renderer->command_allocator->Reset();
    AssertHR(hr);

    // However, when ExecuteCommandList() is called on a particular command 
    // list, that command list can then be reset at any time and must be before 
    // re-recording.
    hr = renderer->command_list->Reset(renderer->command_allocator, renderer->pipeline_state);
    AssertHR(hr);

    // Indicate that the back buffer will be used as a render target.

    D3D12_RESOURCE_BARRIER barrier = {};
    barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
    barrier.Transition.pResource   = renderer->back_buffers[renderer->frame_index];
    barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_PRESENT;
    barrier.Transition.StateAfter  = D3D12_RESOURCE_STATE_RENDER_TARGET;
    renderer->command_list->ResourceBarrier(1, &barrier);

    D3D12_CPU_DESCRIPTOR_HANDLE rtv_handle = renderer->rtv_heap->GetCPUDescriptorHandleForHeapStart();
    rtv_handle.ptr += renderer->frame_index * renderer->rtv_descriptor_size;

    // Record commands.
    float clear_color[] = { 0.0f, 0.2f, 0.4f, 1.0f };
    renderer->command_list->ClearRenderTargetView(rtv_handle, clear_color, 0, NULL);

    // Indicate that the back buffer will now be used to present.
    barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
    barrier.Transition.pResource   = renderer->back_buffers[renderer->frame_index];
    barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_RENDER_TARGET;
    barrier.Transition.StateAfter  = D3D12_RESOURCE_STATE_PRESENT;
    renderer->command_list->ResourceBarrier(1, &barrier);

    hr = renderer->command_list->Close();
    AssertHR(hr);

    // Execute the command list.
    ID3D12CommandList *command_lists[] = { renderer->command_list };
    renderer->command_queue->ExecuteCommandLists(ArrayCount(command_lists), command_lists);

    // Present the frame.
    hr = renderer->swap_chain->Present(1, 0);
    AssertHR(hr);
}

void renderer_d3d12_wait_for_previous_frame(Renderer_D3D12 *renderer) {
    // WAITING FOR THE FRAME TO COMPLETE BEFORE CONTINUING IS NOT BEST PRACTICE.
    // This is code implemented as such for simplicity. The D3D12HelloFrameBuffering
    // sample illustrates how to use fences for efficient resource usage and to
    // maximize GPU utilization.

    // Signal and increment the fence value.
    const UINT64 fence = renderer->fence_value;
    HRESULT hr = renderer->command_queue->Signal(renderer->fence, fence);
    AssertHR(hr);
    renderer->fence_value++;

    // Wait until the previous frame is finished.
    if (renderer->fence->GetCompletedValue() < fence) {
        HRESULT hr = renderer->fence->SetEventOnCompletion(fence, renderer->fence_event);
        AssertHR(hr);
        WaitForSingleObject(renderer->fence_event, INFINITE);
    }

    renderer->frame_index = renderer->swap_chain->GetCurrentBackBufferIndex();
}
