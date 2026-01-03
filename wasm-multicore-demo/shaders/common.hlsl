cbuffer GlobalUniforms : register(b0) {
    float4x4 model;
    float4x4 view;
    float4x4 proj;
    float4x4 view_proj;
    float3 camera_pos;
    float time;
};

struct VertexInput {
    float3 position : TEXCOORD0;
    float3 normal : TEXCOORD1;
    float4 tangent : TEXCOORD2;
    float2 uv : TEXCOORD3;
};

struct VertexOutput {
    float4 position : SV_POSITION;
    float2 uv : TEXCOORD0;
    float3 world_normal : TEXCOORD1;
    float3 world_position : TEXCOORD2;
};

struct PixelInput {
    float4 position : SV_POSITION;
    float2 uv : TEXCOORD0;
    float3 world_normal : TEXCOORD1;
    float3 world_position : TEXCOORD2;
};

static const float3 LIGHT_DIR = float3(0, 0.901, 0.433);
static const float3 LIGHT_COLOR = float3(169.0, 248.0, 255.0) * (1.5 / 255.0);
static const float3 AMBIENT = float3(1.0, 1.0, 1.0) * 0.2;
static const float DIELECTRIC_F0 = 0.04;

float3 pbr_lighting(
    float3 base_color,
    float metallic,
    float smoothness,
    float3 world_normal,
    float3 world_position
) {
    float perceptual_roughness = 1.0 - smoothness;
    float roughness = perceptual_roughness * perceptual_roughness;
    float roughness2 = roughness * roughness;

    float one_minus_reflectivity = (1.0 - DIELECTRIC_F0) * (1.0 - metallic);
    float3 diffuse_color = base_color * one_minus_reflectivity;
    float3 specular_color = lerp(float3(DIELECTRIC_F0, DIELECTRIC_F0, DIELECTRIC_F0), base_color, metallic);

    float3 N = normalize(world_normal);
    float3 V = normalize(camera_pos - world_position);
    float3 L = normalize(LIGHT_DIR);
    float3 H = normalize(L + V);

    float NdotL = max(dot(N, L), 0.0);
    float NdotV = max(dot(N, V), 0.0001);
    float NdotH = max(dot(N, H), 0.0);
    float LdotH = max(dot(L, H), 0.0);

    float d = NdotH * NdotH * (roughness2 - 1.0) + 1.00001;
    float LoH2 = LdotH * LdotH;
    float normalization_term = roughness * 4.0 + 2.0;
    float specular_term = roughness2 / ((d * d) * max(0.1, LoH2) * normalization_term);

    float3 radiance = LIGHT_COLOR * NdotL;
    float3 direct = (diffuse_color + specular_term * specular_color) * radiance;
    float3 ambient = AMBIENT * diffuse_color;

    return ambient + direct;
}
