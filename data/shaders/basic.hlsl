#include "shader_globals.hlsli"

struct Vertex_Output {
    float4 position : SV_POSITION;
    float4 color    : COLOR;
    float2 uv       : TEXCOORD;
    float3 normal   : NORMAL;
    float3 world_position : POSITION;
    float3x3 TBN    : TBNMatrix;
};

Vertex_Output vertex_main(Mesh_Vertex_Input input) {
    Vertex_Output result;

    float4 world_position = mul(world_matrix, float4(input.position, 1.0));
    result.world_position = world_position.xyz;
    result.position = mul(projection_matrix, mul(view_matrix, world_position));
    result.color    = input.color;
    result.uv       = float2(input.uv.x, 1.0 - input.uv.y);
    result.normal   = mul(world_matrix, float4(input.normal, 0.0)).xyz;

    float3 T = normalize(mul(world_matrix, float4(input.tangent, 0.0)).xyz);
    float3 N = normalize(mul(world_matrix, float4(input.normal,  0.0)).xyz);
    T = normalize(T - dot(T, N) * N);
    float3 B = cross(N, T);
    result.TBN = transpose(float3x3(T, B, N));
    
    return result;
}

int calculate_cascade_index(float3 world_position, float3 camera_position) {
    //float depth = length(world_position - camera_position);
    float3 view_pos = mul(view_matrix, float4(world_position, 1.0)).xyz;
    float depth = -view_pos.z;   // because -Z forward

    int index = MAX_SHADOW_CASCADES - 1;
    if (depth < cascade_splits[0].x) index = 0;
    else if (depth < cascade_splits[1].x) index = 1;
    else if (depth < cascade_splits[2].x) index = 2;

    return index;
}

float pcf_shadow(float3 proj_coords, int cascade_index) {
    float shadow = 0.0;
    float total_weight = 0.0;
    
    float current_depth = proj_coords.z;
    float bias = 0.01;
    
    [unroll]
    for (int x = -3; x <= 3; x++) {
        [unroll]
        for (int y = - 3; y <= 3; y++) {
            float s = 0.0;
            switch (cascade_index) {
                case 0: {
                    float2 offset = float2(x, y) / float2(SHADOW_MAP_WIDTH, SHADOW_MAP_HEIGHT);
                    float shadow_depth = shadow_textures[0].Sample(sampler_point, proj_coords.xy + offset).r;
                    s = current_depth - bias > shadow_depth ? 0.0 : 1.0;
                } break;

                case 1: {
                    float2 offset = float2(x, y) / float2(SHADOW_MAP_WIDTH, SHADOW_MAP_HEIGHT);
                    float shadow_depth = shadow_textures[1].Sample(sampler_point, proj_coords.xy + offset).r;
                    s = current_depth - bias > shadow_depth ? 0.0 : 1.0;
                } break;

                case 2: {
                    float2 offset = float2(x, y) / float2(SHADOW_MAP_WIDTH, SHADOW_MAP_HEIGHT);
                    float shadow_depth = shadow_textures[2].Sample(sampler_point, proj_coords.xy + offset).r;
                    s = current_depth - bias > shadow_depth ? 0.0 : 1.0;
                } break;

                case 3: {
                    float2 offset = float2(x, y) / float2(SHADOW_MAP_WIDTH, SHADOW_MAP_HEIGHT);
                    float shadow_depth = shadow_textures[3].Sample(sampler_point, proj_coords.xy + offset).r;
                    s = current_depth - bias > shadow_depth ? 0.0 : 1.0;
                } break;
            }

            float weight = 1.0 / (1.0 + length(float2(x, y)));
            total_weight += weight;
            shadow += s * weight;
        }
    }

    shadow /= total_weight;
    return shadow;
}

float calculate_shadow(int cascade_index, float3 world_position) {
    float4 shadow_pos;

    switch (cascade_index) {
        case 0: shadow_pos  = mul(light_matrix[0], float4(world_position, 1.0)); break;
        case 1: shadow_pos  = mul(light_matrix[1], float4(world_position, 1.0)); break;
        case 2: shadow_pos  = mul(light_matrix[2], float4(world_position, 1.0)); break;
        case 3: shadow_pos  = mul(light_matrix[3], float4(world_position, 1.0)); break;

        default: shadow_pos = float4(0.0, 0.0, 0.0, 1.0); break;
    }
    
    float3 proj_coords = shadow_pos.xyz / shadow_pos.w;
    proj_coords.x = proj_coords.x * 0.5 + 0.5;
    proj_coords.y = proj_coords.y * -0.5 + 0.5;

    float shadow = pcf_shadow(proj_coords, cascade_index);
    return shadow;
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
    float4 full_albedo = albedo_texture.Sample(sampler_linear, input.uv);
    float3 albedo    = full_albedo.rgb * material_albedo_factor.xyz * input.color.rgb;
    
    float3 normal    = input.normal;//get_normal_from_normal_map();

    float4 full_mr = metallic_roughness_texture.Sample(sampler_linear, input.uv);
    float metallic;
    float roughness;
    float3 F0;
    if (uses_specular_glossiness) {
        roughness = 1.0 - full_mr.a;
        metallic  = 0.0;

        F0 = full_mr.rgb;
    } else {
        roughness = full_mr.g;
        metallic  = full_mr.b;

        F0 = float3(0.04, 0.04, 0.04);
        F0 = lerp(F0, albedo, metallic);
    }
    float ao         = ao_texture.Sample(sampler_linear, input.uv).r;
    float3 emissive  = emissive_texture.Sample(sampler_linear, input.uv).rgb * material_emissive_factor;
    
    float3 N = normalize(normal);
    float3 V = normalize(camera_position - input.world_position);

    if (has_normal_map == 1) {
        normal = normal_texture.Sample(sampler_linear, input.uv).xyz;
        normal = normal * 2.0 - 1.0;
        normal = normalize(normal);

        N = normalize(mul(input.TBN, normal));
    }

    int cascade_index = calculate_cascade_index(input.world_position, camera_position);
    float shadow = calculate_shadow(cascade_index, input.world_position);
    
    float3 Lo = float3(0.0, 0.0, 0.0);
    [unroll]
    for (int i = 0; i < MAX_LIGHTS; i++) {
        float3 L;
        float attenuation = 1.0;

        float s = 1.0;
        switch (lights[i].type) {
            case LIGHT_TYPE_DIRECTIONAL: {
                L = normalize(-lights[i].direction);
                s = shadow;
            } break;

            case LIGHT_TYPE_POINT: {
                L = normalize(lights[i].position - input.world_position);
                float dist = length(lights[i].position - input.world_position);
                attenuation = 1.0 / (dist * dist);
            } break;

            case LIGHT_TYPE_SPOT: {
                L = normalize(lights[i].position - input.world_position);
                float dist = length(lights[i].position - input.world_position);
                attenuation = 1.0 / (dist * dist);

                float3 spot_dir  = normalize(-lights[i].direction);
                float  cos_theta = dot(L, spot_dir);
                float  epsilon   = lights[i].spot_inner_cone_angle - lights[i].spot_outer_cone_angle;
                float  intensity = clamp((cos_theta - lights[i].spot_outer_cone_angle) / epsilon, 0.0, 1.0);
                attenuation *= intensity;
            } break;

            default: {
                L = float3(0.0, 0.0, 0.0);
                continue;
            } break;
        }
        
        float3 H   = normalize(V + L);
        float3 radiance = lights[i].color * attenuation * lights[i].intensity;

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
        Lo += ((kD * albedo / PI + specular) * radiance * NdotL) * s;
    }

    float3 ambient = float3(0.03, 0.03, 0.03) * albedo * ao;
    float3 color   = ambient + Lo + emissive;
    
    return float4(color, 1.0);
}
