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

        // Create a RTV and a command allocatr for each frame.
        for (UINT n = 0; n < Renderer::NUM_FRAMES; n++)
        {
            HRESULT hr = renderer->swap_chain->GetBuffer(n, IID_PPV_ARGS(&renderer->back_buffers[n]));
            AssertHR(hr);
            
            renderer->device->CreateRenderTargetView(renderer->back_buffers[n], NULL, rtv_handle);
            rtv_handle.ptr += renderer->rtv_descriptor_size;

            hr = renderer->device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&renderer->command_allocators[n]));
            AssertHR(hr);
        }
    }

    // Create the command list.
    hr = renderer->device->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, renderer->command_allocators[renderer->frame_index], NULL, IID_PPV_ARGS(&renderer->command_list));
    AssertHR(hr);

    // Command lists are created in the recording state, but there is nothing
    // to record yet. The main loop expects it to be closed, so close it now.
    hr = renderer->command_list->Close();
    AssertHR(hr);

    // Create synchronization objects.
    {
        hr = renderer->device->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&renderer->fence));
        AssertHR(hr);
        
        renderer->fence_values[renderer->frame_index]++;

        // Create an event handle to use for frame synchronization.
        renderer->fence_event = CreateEvent(NULL, FALSE, FALSE, NULL);
        if (renderer->fence_event == nullptr) {
            //ThrowIfFailed(HRESULT_FROM_WIN32(GetLastError()));
        }
    }

    // Create an empty root signature.
    {
        D3D12_ROOT_SIGNATURE_DESC root_signature_desc = {};
        root_signature_desc.Flags = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;

        ID3DBlob *signature = NULL, *error = NULL;
        defer { SafeRelease(signature); SafeRelease(error); };
        
        hr = D3D12SerializeRootSignature(&root_signature_desc, D3D_ROOT_SIGNATURE_VERSION_1, &signature, &error);
        AssertHR(hr);
        
        hr = renderer->device->CreateRootSignature(0, signature->GetBufferPointer(), signature->GetBufferSize(), IID_PPV_ARGS(&renderer->root_signature));
        AssertHR(hr);
    }

    // Create the pipeline state, which includes compiling and loading shaders.
    {
        ID3DBlob *vertex_shader = NULL, *pixel_shader = NULL;
        defer { SafeRelease(vertex_shader); SafeRelease(pixel_shader); };

#ifdef BUILD_DEBUG
        // Enable better shader debugging with the graphics debugging tools.
        UINT compile_flags = D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION;
#else
        UINT compile_flags = 0;
#endif

        hr = D3DCompileFromFile(L"data/shaders/basic.hlsl", NULL, NULL, "vertex_main", "vs_5_0", compile_flags, 0, &vertex_shader, NULL);
        AssertHR(hr);
        
        hr = D3DCompileFromFile(L"data/shaders/basic.hlsl", NULL, NULL, "pixel_main", "ps_5_0", compile_flags, 0, &pixel_shader, NULL);
        AssertHR(hr);

        // Define the vertex input layout.
        D3D12_INPUT_ELEMENT_DESC ieds[] =
        {
            { "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
            { "COLOR", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 12, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 }
        };

        // Describe and create the graphics pipeline state object (PSO).
        D3D12_GRAPHICS_PIPELINE_STATE_DESC pso_desc = {};
        pso_desc.InputLayout = { ieds, ArrayCount(ieds) };
        pso_desc.pRootSignature = renderer->root_signature;
        pso_desc.VS = {vertex_shader->GetBufferPointer(), vertex_shader->GetBufferSize()};
        pso_desc.PS = {pixel_shader->GetBufferPointer(), pixel_shader->GetBufferSize()};
        
        pso_desc.RasterizerState.FillMode = D3D12_FILL_MODE_SOLID;
        pso_desc.RasterizerState.CullMode = D3D12_CULL_MODE_BACK;
        pso_desc.RasterizerState.FrontCounterClockwise = FALSE;
        pso_desc.RasterizerState.DepthBias = D3D12_DEFAULT_DEPTH_BIAS;
        pso_desc.RasterizerState.DepthBiasClamp = D3D12_DEFAULT_DEPTH_BIAS_CLAMP;
        pso_desc.RasterizerState.SlopeScaledDepthBias = D3D12_DEFAULT_SLOPE_SCALED_DEPTH_BIAS;
        pso_desc.RasterizerState.DepthClipEnable = TRUE;
        pso_desc.RasterizerState.MultisampleEnable = FALSE;
        pso_desc.RasterizerState.AntialiasedLineEnable = FALSE;
        pso_desc.RasterizerState.ForcedSampleCount = 0;
        pso_desc.RasterizerState.ConservativeRaster = D3D12_CONSERVATIVE_RASTERIZATION_MODE_OFF;
        
        pso_desc.BlendState.AlphaToCoverageEnable = FALSE;
        pso_desc.BlendState.IndependentBlendEnable = FALSE;
        pso_desc.BlendState.RenderTarget[0].BlendEnable = FALSE;
        pso_desc.BlendState.RenderTarget[0].LogicOpEnable = FALSE;
        pso_desc.BlendState.RenderTarget[0].SrcBlend = D3D12_BLEND_ONE;
        pso_desc.BlendState.RenderTarget[0].DestBlend = D3D12_BLEND_ZERO;
        pso_desc.BlendState.RenderTarget[0].BlendOp = D3D12_BLEND_OP_ADD;
        pso_desc.BlendState.RenderTarget[0].SrcBlendAlpha = D3D12_BLEND_ONE;
        pso_desc.BlendState.RenderTarget[0].DestBlendAlpha = D3D12_BLEND_ZERO;
        pso_desc.BlendState.RenderTarget[0].BlendOpAlpha = D3D12_BLEND_OP_ADD;
        pso_desc.BlendState.RenderTarget[0].LogicOp = D3D12_LOGIC_OP_NOOP;
        pso_desc.BlendState.RenderTarget[0].RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;
        
        pso_desc.DepthStencilState.DepthEnable = FALSE;
        pso_desc.DepthStencilState.StencilEnable = FALSE;
        pso_desc.SampleMask = UINT_MAX;
        pso_desc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
        pso_desc.NumRenderTargets = 1;
        pso_desc.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM;
        pso_desc.SampleDesc.Count = 1;

        hr = renderer->device->CreateGraphicsPipelineState(&pso_desc, IID_PPV_ARGS(&renderer->pipeline_state));
        AssertHR(hr);
    }

    // Create the vertex buffer.
    {
        float h = (float)window->height;
        if (h < 1.0f) h = 1.0f;
        float aspect_ratio = window->width / h;
        
        // Define the geometry for a triangle.
        Vertex triangle_vertices[] =
        {
            { { 0.0f, 0.25f * aspect_ratio, 0.0f }, { 1.0f, 0.0f, 0.0f, 1.0f } },
            { { 0.25f, -0.25f * aspect_ratio, 0.0f }, { 0.0f, 1.0f, 0.0f, 1.0f } },
            { { -0.25f, -0.25f * aspect_ratio, 0.0f }, { 0.0f, 0.0f, 1.0f, 1.0f } }
        };

        const UINT vertex_buffer_size = sizeof(triangle_vertices);

        D3D12_HEAP_PROPERTIES heap_properties = {};
        heap_properties.Type = D3D12_HEAP_TYPE_UPLOAD;
        heap_properties.CreationNodeMask = 1;
        heap_properties.VisibleNodeMask = 1;

        D3D12_RESOURCE_DESC resource_desc = {};
        resource_desc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
        resource_desc.Alignment = 0;
        resource_desc.Width = vertex_buffer_size;
        resource_desc.Height = 1;
        resource_desc.DepthOrArraySize = 1;
        resource_desc.MipLevels = 1;
        resource_desc.Format = DXGI_FORMAT_UNKNOWN;
        resource_desc.SampleDesc = { 1, 0 };
        resource_desc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
        resource_desc.Flags = D3D12_RESOURCE_FLAG_NONE;
        
        // Note: using upload heaps to transfer static data like vert buffers is not 
        // recommended. Every time the GPU needs it, the upload heap will be marshalled 
        // over. Please read up on Default Heap usage. An upload heap is used here for 
        // code simplicity and because there are very few verts to actually transfer.
        HRESULT hr = renderer->device->CreateCommittedResource(&heap_properties, D3D12_HEAP_FLAG_NONE, &resource_desc, D3D12_RESOURCE_STATE_GENERIC_READ, NULL, IID_PPV_ARGS(&renderer->vertex_buffer));
        AssertHR(hr);

        // Copy the triangle data to the vertex buffer.
        UINT8 *vertex_data_begin;
        D3D12_RANGE read_range = {}; // We do not intend to read from this resource on the CPU.

        hr = renderer->vertex_buffer->Map(0, &read_range, (void**)&vertex_data_begin);
        AssertHR(hr);
        
        memcpy(vertex_data_begin, triangle_vertices, sizeof(triangle_vertices));
        
        renderer->vertex_buffer->Unmap(0, NULL);

        // Initialize the vertex buffer view.
        renderer->vertex_buffer_view.BufferLocation = renderer->vertex_buffer->GetGPUVirtualAddress();
        renderer->vertex_buffer_view.StrideInBytes  = sizeof(Vertex);
        renderer->vertex_buffer_view.SizeInBytes    = vertex_buffer_size;
    }

    renderer->viewport = {};
    renderer->viewport.Width    = (float)window->width;
    renderer->viewport.Height   = (float)window->height;
    renderer->viewport.MaxDepth = 1.0f;

    renderer->scissor_rect = {};
    renderer->scissor_rect.right  = window->width;
    renderer->scissor_rect.bottom = window->height;
    
    renderer_d3d12_wait_for_gpu(renderer);
    
    return renderer;
}

void renderer_d3d12_shutdown(Renderer_D3D12 *renderer) {
    renderer_d3d12_wait_for_gpu(renderer);

    CloseHandle(renderer->fence_event);
}

void renderer_d3d12_execute_render_commands(Renderer_D3D12 *renderer) {
    // Command list allocators can only be reset when the associated 
    // command lists have finished execution on the GPU; apps should use 
    // fences to determine GPU execution progress.
    HRESULT hr = renderer->command_allocators[renderer->frame_index]->Reset();
    AssertHR(hr);

    // However, when ExecuteCommandList() is called on a particular command 
    // list, that command list can then be reset at any time and must be before 
    // re-recording.
    hr = renderer->command_list->Reset(renderer->command_allocators[renderer->frame_index], renderer->pipeline_state);
    AssertHR(hr);
    
    renderer->command_list->SetGraphicsRootSignature(renderer->root_signature);
    renderer->command_list->RSSetViewports(1, &renderer->viewport);
    renderer->command_list->RSSetScissorRects(1, &renderer->scissor_rect);

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
    renderer->command_list->OMSetRenderTargets(1, &rtv_handle, FALSE, NULL);
    float clear_color[] = { 0.0f, 0.2f, 0.4f, 1.0f };
    renderer->command_list->ClearRenderTargetView(rtv_handle, clear_color, 0, NULL);
    renderer->command_list->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    renderer->command_list->IASetVertexBuffers(0, 1, &renderer->vertex_buffer_view);
    renderer->command_list->DrawInstanced(3, 1, 0, 0);
    
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

void renderer_d3d12_move_to_next_frame(Renderer_D3D12 *renderer) {
    // Schedule a Signal command in the queue.
    const UINT64 current_fence_value = renderer->fence_values[renderer->frame_index];
    HRESULT hr = renderer->command_queue->Signal(renderer->fence, current_fence_value);
    AssertHR(hr);

    // Update the frame index.
    renderer->frame_index = renderer->swap_chain->GetCurrentBackBufferIndex();

    // If the next frame is not ready to be rendered yet, wait until it is ready.
    if (renderer->fence->GetCompletedValue() < renderer->fence_values[renderer->frame_index]) {
        hr = renderer->fence->SetEventOnCompletion(renderer->fence_values[renderer->frame_index], renderer->fence_event);
        AssertHR(hr);
    
        WaitForSingleObjectEx(renderer->fence_event, INFINITE, FALSE);
    }

    // Set the fence value for the next frame.
    renderer->fence_values[renderer->frame_index] = current_fence_value + 1;
}

void renderer_d3d12_wait_for_gpu(Renderer_D3D12 *renderer) {
    // Schedule a Signal command in the queue.
    HRESULT hr = renderer->command_queue->Signal(renderer->fence, renderer->fence_values[renderer->frame_index]);
    AssertHR(hr);

    // Wait until the fence has been processed.
    hr = renderer->fence->SetEventOnCompletion(renderer->fence_values[renderer->frame_index], renderer->fence_event);
    WaitForSingleObjectEx(renderer->fence_event, INFINITE, FALSE);

    // Increment the fence value for the current frame.
    renderer->fence_values[renderer->frame_index]++;
}
