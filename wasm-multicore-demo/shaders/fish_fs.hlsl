#include "fog.hlsl"

cbuffer MaterialUniforms : register(b1) {
    float4 tint_color;
    float tint_offset;
    float metallic;
    float smoothness;
    float wave_frequency;
    float wave_speed;
    float wave_distance;
    float wave_offset;
};

Texture2D albedo_texture : register(t1);
SamplerState albedo_sampler : register(s0);
Texture2D tint_texture : register(t3);
SamplerState tint_sampler : register(s2);
Texture2D metallic_gloss_texture : register(t5);
SamplerState metallic_gloss_sampler : register(s4);

float4 ps_main(PixelInput input) : SV_TARGET {
    float4 albedo_sample = albedo_texture.Sample(albedo_sampler, input.uv);
    float2 tint_uv = float2(tint_offset, 0.0);
    float3 tint_sample = tint_texture.Sample(tint_sampler, tint_uv).rgb;
    float4 metallic_gloss = metallic_gloss_texture.Sample(metallic_gloss_sampler, input.uv);

    float3 tinted = albedo_sample.rgb * tint_sample;
    float3 base_color = lerp(albedo_sample.rgb, tinted, albedo_sample.a) * tint_color.rgb;
    float final_smoothness = metallic_gloss.a * smoothness;

    float3 final_color = pbr_lighting(base_color, metallic, final_smoothness, input.world_normal, input.world_position);
    return float4(apply_fog(final_color, input.world_position), 1.0);
}
