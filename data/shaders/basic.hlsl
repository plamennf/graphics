#include "shader_globals.hlsli"

struct Vertex_Output {
    float4 position : SV_POSITION;
    float2 uv       : TEXCOORD;
    float3 normal   : NORMAL;
    float3 world_position : POSITION;
};

Vertex_Output vertex_main(Mesh_Vertex_Input input) {
    Vertex_Output result;

    float4 world_position = mul(world_matrix, float4(input.position, 1.0));
    result.world_position = world_position.xyz;
    result.position = mul(projection_matrix, mul(view_matrix, world_position));
    result.uv       = float2(input.uv.x, 1.0 - input.uv.y);
    result.normal   = input.normal;
    
    return result;
}

float distribution_ggx(float3 N, float3 H, float roughness) {
    float a      = roughness * roughness;
    float a2     = a * a;
    float NdotH  = max(dot(N, H), 0.0);
    float NdotH2 = NdotH * NdotH;

    float num    = a2;
    float denom  = (NdotH2 * (a2 - 1.0) + 1.0);
    denom        = PI * denom * denom;

    return num / denom;
}

float geometry_schlick_ggx(float NdotV, float roughness) {
    float r     = (roughness + 1.0);
    float k     = (r*r) / 8.0;

    float num   = NdotV;
    float denom = NdotV * (1.0 - k) + k;

    return num / denom;
}

float geometry_smith(float3 N, float3 V, float3 L, float roughness) {
    float NdotV = max(dot(N, V), 0.0);
    float NdotL = max(dot(N, L), 0.0);
    float ggx2  = geometry_schlick_ggx(NdotV, roughness);
    float ggx1  = geometry_schlick_ggx(NdotL, roughness);

    return ggx1 * ggx2;
}

float3 fresnel_schlick(float cos_theta, float3 F0) {
    return F0 + (1.0 - F0) * pow(clamp(1.0 - cos_theta, 0.0, 1.0), 5.0);
}  

// From https://learnopengl.com/PBR/Lighting
float4 pixel_main(Vertex_Output input) : SV_TARGET {
    float3 albedo    = albedo_texture.Sample(sampler_linear, input.uv).rgb;
    float3 normal    = input.normal;//get_normal_from_normal_map();
    float metallic   = metallic_roughness_texture.Sample(sampler_linear, input.uv).b;
    float roughness  = metallic_roughness_texture.Sample(sampler_linear, input.uv).g;
    float ao         = ao_texture.Sample(sampler_linear, input.uv).r;

    float3 N = normalize(normal);
    float3 V = normalize(camera_position - input.world_position);

    float3 F0 = float3(0.04, 0.04, 0.04);
    F0 = lerp(F0, albedo, metallic);

    float3 Lo = float3(0.0, 0.0, 0.0);
    [unroll]
    for (int i = 0; i < MAX_LIGHTS; i++) {
        float3 L   = normalize(lights[i].position - input.world_position);
        float3 H   = normalize(V + L);
        float dist = length(lights[i].position - input.world_position);
        float attenuation = 1.0 / (dist * dist);
        float3 radiance = lights[i].color * attenuation;

        float NDF = distribution_ggx(N, H, roughness);
        float G   = geometry_smith(N, V, L, roughness);
        float3 F  = fresnel_schlick(max(dot(H, V), 0.0), F0);

        float3 kS = F;
        float3 kD = float3(1.0, 1.0, 1.0) - kS;
        kD *= 1.0 - metallic;

        float3 numerator  = NDF * G * F;
        float denominator = 4.0 * max(dot(N, V), 0.0) * max(dot(N, L), 0.0) + 0.0001;
        float3 specular   = numerator / denominator;

        float NdotL = max(dot(N, L), 0.0);
        Lo += (kD * albedo / PI + specular) * radiance * NdotL;
    }

    float3 ambient = float3(0.03, 0.03, 0.03) * albedo * ao;
    float3 color   = ambient + Lo;
    //color = color / (color + float3(1.0));
    
    return float4(color, 1.0);
}
