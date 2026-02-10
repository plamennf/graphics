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

        D3D12_DESCRIPTOR_HEAP_DESC srv_heap_desc = {};
        srv_heap_desc.NumDescriptors = 100000; // 100 000
        srv_heap_desc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
        srv_heap_desc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
        hr = renderer->device->CreateDescriptorHeap(&srv_heap_desc, IID_PPV_ARGS(&renderer->srv_heap));
        AssertHR(hr);

        renderer->srv_descriptor_size = renderer->device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
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

    //
    // Create per scene constant buffer
    //
    {
        D3D12_HEAP_PROPERTIES heap_properties = {};
        heap_properties.Type = D3D12_HEAP_TYPE_UPLOAD;
        heap_properties.CreationNodeMask = 1;
        heap_properties.VisibleNodeMask = 1;

        D3D12_RESOURCE_DESC resource_desc = {};
        resource_desc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
        resource_desc.Alignment = 0;
        resource_desc.Width = sizeof(Per_Scene_Uniforms);
        resource_desc.Height = 1;
        resource_desc.DepthOrArraySize = 1;
        resource_desc.MipLevels = 1;
        resource_desc.Format = DXGI_FORMAT_UNKNOWN;
        resource_desc.SampleDesc = { 1, 0 };
        resource_desc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
        resource_desc.Flags = D3D12_RESOURCE_FLAG_NONE;
        
        HRESULT hr = renderer->device->CreateCommittedResource(&heap_properties, D3D12_HEAP_FLAG_NONE, &resource_desc, D3D12_RESOURCE_STATE_GENERIC_READ, NULL, IID_PPV_ARGS(&renderer->per_scene_cb));
        AssertHR(hr);

        // Describe and create a constant buffer view.
        D3D12_CONSTANT_BUFFER_VIEW_DESC cbv_desc = {};
        cbv_desc.BufferLocation = renderer->per_scene_cb->GetGPUVirtualAddress();
        cbv_desc.SizeInBytes = sizeof(Per_Scene_Uniforms);

        D3D12_CPU_DESCRIPTOR_HANDLE cpu_descriptor_handle = renderer->srv_heap->GetCPUDescriptorHandleForHeapStart();
        cpu_descriptor_handle.ptr += renderer->srv_descriptor_size * renderer->num_allocated_textures;
        renderer->device->CreateConstantBufferView(&cbv_desc, cpu_descriptor_handle);

        // Map and initialize the constant buffer. We don't unmap this until the
        // app closes. Keeping things mapped for the lifetime of the resource is okay.
        D3D12_RANGE read_range = {};
        hr = renderer->per_scene_cb->Map(0, &read_range, (void**)&renderer->per_scene_cb_data);
        AssertHR(hr);

        renderer->per_scene_cb_descriptor_handle = renderer->srv_heap->GetGPUDescriptorHandleForHeapStart();
        renderer->per_scene_cb_descriptor_handle.ptr += renderer->num_allocated_textures * renderer->srv_descriptor_size;

        renderer->num_allocated_textures += 1;
    }

    renderer->viewport = {};
    renderer->viewport.Width    = (float)window->width;
    renderer->viewport.Height   = (float)window->height;
    renderer->viewport.MaxDepth = 1.0f;

    renderer->scissor_rect = {};
    renderer->scissor_rect.right  = window->width;
    renderer->scissor_rect.bottom = window->height;

    renderer->max_push_buffer_size = Kilobytes(64);
    renderer->push_buffer_base     = MaAllocArray(&globals.permanent_memory, u8, renderer->max_push_buffer_size);
    renderer->push_buffer_data_at  = renderer->push_buffer_base;

    renderer->gpu_resources_memory.size     = Kilobytes(128);
    renderer->gpu_resources_memory.occupied = 0;
    renderer->gpu_resources_memory.data     = MaAllocArray(&globals.permanent_memory, u8, renderer->gpu_resources_memory.size);
    
    renderer_d3d12_wait_for_gpu(renderer);

    return renderer;
}

void renderer_d3d12_shutdown(Renderer_D3D12 *renderer) {
    renderer_d3d12_wait_for_gpu(renderer);

    CloseHandle(renderer->fence_event);
}

void renderer_d3d12_execute_render_commands_and_present(Renderer_D3D12 *renderer) {
    // Command list allocators can only be reset when the associated 
    // command lists have finished execution on the GPU; apps should use 
    // fences to determine GPU execution progress.
    HRESULT hr = renderer->command_allocators[renderer->frame_index]->Reset();
    AssertHR(hr);

    // However, when ExecuteCommandList() is called on a particular command 
    // list, that command list can then be reset at any time and must be before 
    // re-recording.
    hr = renderer->command_list->Reset(renderer->command_allocators[renderer->frame_index], NULL);
    AssertHR(hr);
    
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

    ID3D12DescriptorHeap *descriptor_heaps[] = {
        renderer->srv_heap,
    };
    renderer->command_list->SetDescriptorHeaps(ArrayCount(descriptor_heaps), descriptor_heaps);
    
    for (u8 *at = renderer->push_buffer_base; at < renderer->push_buffer_data_at;) {
        Render_Entry_Type type = *(Render_Entry_Type *)at;
        at++;

        switch (type) {
            case RET_Per_Scene_Uniforms: {
                auto entry = (Per_Scene_Uniforms *)at;
                at += sizeof(*entry);

                memcpy(renderer->per_scene_cb_data, entry, sizeof(Per_Scene_Uniforms));
            } break;
            
            case RET_Render_Entry_Draw_Item: {
                auto entry = (Render_Entry_Draw_Item *)at;
                at += sizeof(*entry);

                Draw_Item_Info info = entry->info;

                Shader_D3D12 *shader = (Shader_D3D12 *)info.shader;
                Assert(shader);
                
                Gpu_Buffer_D3D12 *vertex_buffer = (Gpu_Buffer_D3D12 *)info.vertex_buffer;
                Assert(vertex_buffer);
                
                Gpu_Buffer_D3D12 *index_buffer = (Gpu_Buffer_D3D12 *)info.index_buffer;

                Texture_D3D12 *texture = (Texture_D3D12 *)info.texture;
                Assert(texture);
                
                renderer->command_list->SetPipelineState(shader->pipeline_state);
                renderer->command_list->SetGraphicsRootSignature(shader->root_signature);
                renderer->command_list->SetGraphicsRootDescriptorTable(0, texture->descriptor_handle);
                renderer->command_list->SetGraphicsRootDescriptorTable(1, renderer->per_scene_cb_descriptor_handle);
                
                renderer->command_list->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

                renderer->command_list->IASetVertexBuffers(0, 1, &vertex_buffer->vertex_buffer_view);
                if (index_buffer) {
                    renderer->command_list->IASetIndexBuffer(&index_buffer->index_buffer_view);
                    renderer->command_list->DrawIndexedInstanced(info.num_indices, 1, info.first_index, 0, 0);
                } else {
                    renderer->command_list->DrawInstanced(info.num_indices, 1, info.first_index, 0);
                }
            } break;
        }
    }
    
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

    renderer->push_buffer_data_at = renderer->push_buffer_base;
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

Shader *renderer_d3d12_load_shader(Renderer_D3D12 *renderer, Shader_Info info) {
#if 1

    D3D12_FEATURE_DATA_ROOT_SIGNATURE feature_data = {};

    // This is the highest version the sample supports. If CheckFeatureSupport succeeds, the HighestVersion returned will not be greater than this.
    feature_data.HighestVersion = D3D_ROOT_SIGNATURE_VERSION_1_1;

    if (FAILED(renderer->device->CheckFeatureSupport(D3D12_FEATURE_ROOT_SIGNATURE, &feature_data, sizeof(feature_data)))) {
        feature_data.HighestVersion = D3D_ROOT_SIGNATURE_VERSION_1_0;
    }

    s64 mark = get_temporary_storage_mark();
    defer { set_temporary_storage_mark(mark); };

    D3D12_DESCRIPTOR_RANGE1 ranges[2] = {};

    ranges[0].RangeType      = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
    ranges[0].NumDescriptors = 1;
    ranges[0].Flags          = D3D12_DESCRIPTOR_RANGE_FLAG_DATA_STATIC;
    ranges[0].BaseShaderRegister = 0;
    ranges[0].RegisterSpace      = 0;
    ranges[0].OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;

    ranges[1].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_CBV;
    ranges[1].NumDescriptors = 1;
    ranges[1].Flags          = D3D12_DESCRIPTOR_RANGE_FLAG_NONE;
    ranges[1].BaseShaderRegister = 0;
    ranges[1].RegisterSpace      = 0;
    ranges[1].OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;
    
    D3D12_ROOT_PARAMETER1 root_parameters[2] = {};

    root_parameters[0].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
    root_parameters[0].DescriptorTable.NumDescriptorRanges = 1;
    root_parameters[0].DescriptorTable.pDescriptorRanges   = &ranges[0];
    root_parameters[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;

    root_parameters[1].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
    root_parameters[1].DescriptorTable.NumDescriptorRanges = 1;
    root_parameters[1].DescriptorTable.pDescriptorRanges   = &ranges[1];
    root_parameters[1].ShaderVisibility = D3D12_SHADER_VISIBILITY_VERTEX;
    
    auto samplers = TAllocArray(D3D12_STATIC_SAMPLER_DESC, info.num_static_samplers);

    for (int i = 0; i < info.num_static_samplers; i++) {
        D3D12_STATIC_SAMPLER_DESC sampler = {};

        switch (info.static_samplers[i].filter) {
            case TEXTURE_FILTER_POINT: {
                sampler.Filter = D3D12_FILTER_MIN_MAG_MIP_POINT;
            } break;

            case TEXTURE_FILTER_LINEAR: {
                sampler.Filter = D3D12_FILTER_MIN_MAG_MIP_LINEAR;
            } break;
        }

        switch (info.static_samplers[i].wrap) {
            case TEXTURE_WRAP_REPEAT: {
                sampler.AddressU = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
                sampler.AddressV = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
                sampler.AddressW = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
            } break;

            case TEXTURE_WRAP_CLAMP: {
                sampler.AddressU = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
                sampler.AddressV = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
                sampler.AddressW = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
            } break;
        }
        
        sampler.MipLODBias = 0;
        sampler.MaxAnisotropy = 0;
        sampler.ComparisonFunc = D3D12_COMPARISON_FUNC_NEVER;
        sampler.BorderColor = D3D12_STATIC_BORDER_COLOR_TRANSPARENT_BLACK;
        sampler.MinLOD = 0.0f;
        sampler.MaxLOD = D3D12_FLOAT32_MAX;
        sampler.ShaderRegister = 0;
        sampler.RegisterSpace = 0;
        sampler.ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;

        samplers[i] = sampler;
    }

    // TODO: If Root Signature Version 1.1 is not supported, we need to create
    // a Root Signature Version 1.0 one.
    
    D3D12_VERSIONED_ROOT_SIGNATURE_DESC root_signature_desc;

    root_signature_desc.Version = D3D_ROOT_SIGNATURE_VERSION_1_1;
    root_signature_desc.Desc_1_1.NumParameters     = ArrayCount(root_parameters);
    root_signature_desc.Desc_1_1.pParameters       = root_parameters;
    root_signature_desc.Desc_1_1.NumStaticSamplers = info.num_static_samplers;
    root_signature_desc.Desc_1_1.pStaticSamplers   = samplers;
    root_signature_desc.Desc_1_1.Flags             = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;
    
    ID3DBlob *signature = NULL, *error = NULL;
    defer { SafeRelease(signature); SafeRelease(error); };

    HRESULT hr = D3D12SerializeVersionedRootSignature(&root_signature_desc, &signature, &error);
    AssertHR(hr);

    ID3D12RootSignature *root_signature = NULL;
    hr = renderer->device->CreateRootSignature(0, signature->GetBufferPointer(), signature->GetBufferSize(), IID_PPV_ARGS(&root_signature));
    AssertHR(hr);
    
#else
// Create an empty root signature.
    D3D12_ROOT_SIGNATURE_DESC root_signature_desc = {};
    root_signature_desc.Flags = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;

    ID3DBlob *signature = NULL, *error = NULL;
    defer { SafeRelease(signature); SafeRelease(error); };
        
    HRESULT hr = D3D12SerializeRootSignature(&root_signature_desc, D3D_ROOT_SIGNATURE_VERSION_1, &signature, &error);
    AssertHR(hr);

    ID3D12RootSignature *root_signature = NULL;
    hr = renderer->device->CreateRootSignature(0, signature->GetBufferPointer(), signature->GetBufferSize(), IID_PPV_ARGS(&root_signature));
    AssertHR(hr);
#endif

    // Create the pipeline state, which includes compiling and loading shaders.
    ID3DBlob *vertex_shader = NULL, *pixel_shader = NULL;
    defer { SafeRelease(vertex_shader); SafeRelease(pixel_shader); };

#ifdef BUILD_DEBUG
    // Enable better shader debugging with the graphics debugging tools.
    UINT compile_flags = D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION;
#else
    UINT compile_flags = 0;
#endif

    wchar_t wide_filepath[4096] = {};
    MultiByteToWideChar(CP_UTF8, 0, info.filepath.data, info.filepath.length, wide_filepath, ArrayCount(wide_filepath));
    for (wchar_t *at = wide_filepath; *at; at++) {
        if (*at == L'/') {
            *at = L'\\';
        }
    }
    
    hr = D3DCompileFromFile(wide_filepath, NULL, NULL, "vertex_main", "vs_5_0", compile_flags, 0, &vertex_shader, NULL);
    AssertHR(hr);
        
    hr = D3DCompileFromFile(wide_filepath, NULL, NULL, "pixel_main", "ps_5_0", compile_flags, 0, &pixel_shader, NULL);
    AssertHR(hr);

    D3D12_INPUT_ELEMENT_DESC ieds[16] = {};
    UINT num_ieds = 0;

    switch (info.render_vertex_type) {
        case RENDER_VERTEX_TYPE_IMMEDIATE: {
            num_ieds = 3;
            
            ieds[0].SemanticName      = "POSITION";
            ieds[0].Format            = DXGI_FORMAT_R32G32_FLOAT;
            ieds[0].AlignedByteOffset = offsetof(Immediate_Vertex, position);
            ieds[0].InputSlotClass    = D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA;

            ieds[1].SemanticName      = "COLOR";
            ieds[1].Format            = DXGI_FORMAT_R32G32B32A32_FLOAT;
            ieds[1].AlignedByteOffset = offsetof(Immediate_Vertex, color);
            ieds[1].InputSlotClass    = D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA;

            ieds[2].SemanticName      = "TEXCOORD";
            ieds[2].Format            = DXGI_FORMAT_R32G32_FLOAT;
            ieds[2].AlignedByteOffset = offsetof(Immediate_Vertex, uv);
            ieds[2].InputSlotClass    = D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA;
        } break;
    }

    D3D12_GRAPHICS_PIPELINE_STATE_DESC pso_desc = {};
    pso_desc.InputLayout    = { ieds, num_ieds };
    pso_desc.pRootSignature = root_signature;
    pso_desc.VS = {vertex_shader->GetBufferPointer(), vertex_shader->GetBufferSize()};
    pso_desc.PS = {pixel_shader->GetBufferPointer(), pixel_shader->GetBufferSize()};
        
    pso_desc.RasterizerState.FillMode = D3D12_FILL_MODE_SOLID;
    pso_desc.RasterizerState.CullMode = D3D12_CULL_MODE_BACK;
    pso_desc.RasterizerState.FrontCounterClockwise = TRUE;
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

    ID3D12PipelineState *pipeline_state = NULL;
    hr = renderer->device->CreateGraphicsPipelineState(&pso_desc, IID_PPV_ARGS(&pipeline_state));
    AssertHR(hr);

    Shader_D3D12 *result   = MaAllocStruct(&renderer->gpu_resources_memory, Shader_D3D12);

    result->info           = info;
    result->root_signature = root_signature;
    result->pipeline_state = pipeline_state;
    
    return result;
}

Gpu_Buffer *renderer_d3d12_allocate_buffer(Renderer_D3D12 *renderer, Gpu_Buffer_Type type, u32 size, u32 stride, void *initial_data) {
    Assert(size);
    
    D3D12_HEAP_PROPERTIES heap_properties = {};
    heap_properties.Type = D3D12_HEAP_TYPE_UPLOAD;
    heap_properties.CreationNodeMask = 1;
    heap_properties.VisibleNodeMask = 1;

    D3D12_RESOURCE_DESC resource_desc = {};
    resource_desc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
    resource_desc.Alignment = 0;
    resource_desc.Width = size;
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
    ID3D12Resource *resource = NULL;
    HRESULT hr = renderer->device->CreateCommittedResource(&heap_properties, D3D12_HEAP_FLAG_NONE, &resource_desc, D3D12_RESOURCE_STATE_GENERIC_READ, NULL, IID_PPV_ARGS(&resource));
    AssertHR(hr);

    if (initial_data) {
        UINT8 *data_begin;
        D3D12_RANGE read_range = {}; // We do not intend to read from this resource on the CPU.

        hr = resource->Map(0, &read_range, (void **)&data_begin);
        AssertHR(hr);

        memcpy(data_begin, initial_data, size);

        resource->Unmap(0, NULL);
    }

    D3D12_VERTEX_BUFFER_VIEW vertex_buffer_view = {};
    D3D12_INDEX_BUFFER_VIEW index_buffer_view = {};
    switch (type) {
        case GPU_BUFFER_TYPE_VERTEX_BUFFER: {
            vertex_buffer_view.BufferLocation = resource->GetGPUVirtualAddress();
            vertex_buffer_view.StrideInBytes  = stride;
            vertex_buffer_view.SizeInBytes    = size;
        } break;

        case GPU_BUFFER_TYPE_INDEX_BUFFER: {
            index_buffer_view.BufferLocation = resource->GetGPUVirtualAddress();
            index_buffer_view.SizeInBytes    = size;
            index_buffer_view.Format         = DXGI_FORMAT_R32_UINT;
        } break;
    }
    
    Gpu_Buffer_D3D12 *result = MaAllocStruct(&renderer->gpu_resources_memory, Gpu_Buffer_D3D12);

    result->type   = type;
    result->size   = size;
    result->stride = stride;

    result->resource           = resource;
    result->vertex_buffer_view = vertex_buffer_view;
    result->index_buffer_view  = index_buffer_view;

    return result;
}

Texture *renderer_d3d12_allocate_texture(Renderer_D3D12 *renderer, int width, int height, Texture_Format format, int bpp, void *pixels) {
    // Note: ComPtr's are CPU objects but this resource needs to stay in scope until
    // the command list that references it has finished executing on the GPU.
    // We will flush the GPU at the end of this method to ensure the resource is not
    // prematurely destroyed.
    ID3D12Resource *texture_upload_heap;
    defer { SafeRelease(texture_upload_heap); };

    ID3D12Resource *texture = NULL;
    D3D12_GPU_DESCRIPTOR_HANDLE texture_descriptor_handle = {};

    // Command list allocators can only be reset when the associated 
    // command lists have finished execution on the GPU; apps should use 
    // fences to determine GPU execution progress.
    HRESULT hr = renderer->command_allocators[renderer->frame_index]->Reset();
    AssertHR(hr);

    // However, when ExecuteCommandList() is called on a particular command 
    // list, that command list can then be reset at any time and must be before 
    // re-recording.
    hr = renderer->command_list->Reset(renderer->command_allocators[renderer->frame_index], NULL);
    AssertHR(hr);
    
    // Create the texture.
    {
        // Describe and create a Texture2D.
        D3D12_RESOURCE_DESC texture_desc = {};
        texture_desc.MipLevels = 1;

        switch (format) {
            case TEXTURE_FORMAT_RGBA8: {
                texture_desc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
            } break;
        }

        texture_desc.Width = width;
        texture_desc.Height = height;
        texture_desc.Flags = D3D12_RESOURCE_FLAG_NONE;
        texture_desc.DepthOrArraySize = 1;
        texture_desc.SampleDesc.Count = 1;
        texture_desc.SampleDesc.Quality = 0;
        texture_desc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;

        D3D12_HEAP_PROPERTIES heap_properties = {};
        heap_properties.Type = D3D12_HEAP_TYPE_DEFAULT;
        heap_properties.CreationNodeMask = 1;
        heap_properties.VisibleNodeMask = 1;
        
        HRESULT hr = renderer->device->CreateCommittedResource(&heap_properties, D3D12_HEAP_FLAG_NONE, &texture_desc, D3D12_RESOURCE_STATE_COPY_DEST, NULL, IID_PPV_ARGS(&texture));
        AssertHR(hr);

        UINT64 required_size = 0;
        D3D12_PLACED_SUBRESOURCE_FOOTPRINT layout = {};
        UINT num_rows = 0;
        UINT64 row_sizes = 0;

        renderer->device->GetCopyableFootprints(&texture_desc, 0, 1, 0, &layout, &num_rows, &row_sizes, &required_size);

        const UINT64 upload_buffer_size = required_size;
        
        heap_properties.Type = D3D12_HEAP_TYPE_UPLOAD;

        D3D12_RESOURCE_DESC resource_desc = {};
        resource_desc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
        resource_desc.Alignment = 0;
        resource_desc.Width = upload_buffer_size;
        resource_desc.Height = 1;
        resource_desc.DepthOrArraySize = 1;
        resource_desc.MipLevels = 1;
        resource_desc.Format = DXGI_FORMAT_UNKNOWN;
        resource_desc.SampleDesc = { 1, 0 };
        resource_desc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
        resource_desc.Flags = D3D12_RESOURCE_FLAG_NONE;
        
        // Create the GPU upload buffer.
        hr = renderer->device->CreateCommittedResource(&heap_properties,  D3D12_HEAP_FLAG_NONE, &resource_desc, D3D12_RESOURCE_STATE_GENERIC_READ, NULL, IID_PPV_ARGS(&texture_upload_heap));
        AssertHR(hr);

        // Copy data to the intermediate upload heap and then schedule a copy 
        // from the upload heap to the Texture2D.

        D3D12_SUBRESOURCE_DATA texture_data = {};
        texture_data.pData = pixels;
        texture_data.RowPitch = width * bpp;
        texture_data.SlicePitch = texture_data.RowPitch * height;

        //UpdateSubresources(m_commandList.Get(), m_texture.Get(), textureUploadHeap.Get(), 0, 0, 1, &textureData);

        {
            UINT8 *mapped_data;
            D3D12_RANGE read_range = {};

            hr = texture_upload_heap->Map(0, &read_range, (void **)&mapped_data);
            AssertHR(hr);

            {
                UINT8 *dest   = mapped_data + layout.Offset;
                UINT8 *source = (UINT8 *)texture_data.pData;

                for (UINT row = 0; row < num_rows; row++) {
                    memcpy(dest + row * layout.Footprint.RowPitch, source + row * texture_data.RowPitch, row_sizes);
                }
            }
                
            texture_upload_heap->Unmap(0, NULL);

            {
                D3D12_TEXTURE_COPY_LOCATION dest = {};
                dest.pResource        = texture;
                dest.Type             = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
                dest.SubresourceIndex = 0;

                D3D12_TEXTURE_COPY_LOCATION source = {};
                source.pResource       = texture_upload_heap;
                source.Type            = D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT;
                source.PlacedFootprint = layout;

                renderer->command_list->CopyTextureRegion(&dest, 0, 0, 0, &source, NULL);
            }
        }

        D3D12_RESOURCE_BARRIER barrier = {};
        barrier.Type  = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
        //barrier.Flags = ;
        barrier.Transition.pResource   = texture;
        barrier.Transition.Subresource = 0;
        barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_COPY_DEST;
        barrier.Transition.StateAfter  = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
        
        renderer->command_list->ResourceBarrier(1, &barrier);

        // Describe and create a SRV for the texture.
        D3D12_SHADER_RESOURCE_VIEW_DESC srv_desc = {};
        srv_desc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
        srv_desc.Format = texture_desc.Format;
        srv_desc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
        srv_desc.Texture2D.MipLevels = 1;

        // TODO: Get the srv_descriptor_size and keep track of how many textures were created and give the corresponding handle for the first free texture slot(in case some of the textures were freed).

        D3D12_CPU_DESCRIPTOR_HANDLE texture_descriptor_handle = renderer->srv_heap->GetCPUDescriptorHandleForHeapStart();
        texture_descriptor_handle.ptr += renderer->num_allocated_textures * renderer->srv_descriptor_size;
        renderer->device->CreateShaderResourceView(texture, &srv_desc, texture_descriptor_handle);
    }

    renderer_d3d12_wait_for_gpu(renderer);
    
    // Close the command list and execute it to begin the initial GPU setup.
    hr = renderer->command_list->Close();
    AssertHR(hr);
    
    ID3D12CommandList *command_lists[] = { renderer->command_list };
    renderer->command_queue->ExecuteCommandLists(ArrayCount(command_lists), command_lists);

    renderer_d3d12_wait_for_gpu(renderer);
    
    auto result = MaAllocStruct(&renderer->gpu_resources_memory, Texture_D3D12);

    result->width  = width;
    result->height = height;
    result->format = format;
    result->bpp    = bpp;

    result->resource = texture;
    result->descriptor_handle = renderer->srv_heap->GetGPUDescriptorHandleForHeapStart();
    result->descriptor_handle.ptr += renderer->num_allocated_textures * renderer->srv_descriptor_size;

    renderer->num_allocated_textures++;
    
    return result;
}
