#pragma once

struct Gpu_Buffer_Platform_Specific {
    struct ID3D11Buffer *buffer;
};

struct Texture_Platform_Specific {
    struct ID3D11Texture2D *texture;
    struct ID3D11ShaderResourceView *srv;
    struct ID3D11RenderTargetView *rtv;
    struct ID3D11DepthStencilView *dsv;
};

struct Shader {
    struct ID3D11VertexShader *vertex_shader;
    struct ID3D11PixelShader  *pixel_shader;
    struct ID3D11InputLayout  *input_layout;
};

struct Command_Buffer_Platform_Specific {
    struct ID3D11DeviceContext *context;
};
