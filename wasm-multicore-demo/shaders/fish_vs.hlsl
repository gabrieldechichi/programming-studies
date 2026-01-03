#include "common.hlsl"

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

VertexOutput vs_main(VertexInput input) {
    VertexOutput output;

    float wiggle_input = (time + wave_offset) * wave_speed + input.position.y * wave_frequency;
    float x_offset = sin(wiggle_input) * wave_distance;
    float3 wiggled_pos = float3(input.position.x + x_offset, input.position.y, input.position.z);

    float4 world_pos = mul(model, float4(wiggled_pos, 1.0));
    output.position = mul(view_proj, world_pos);
    output.world_position = world_pos.xyz;
    output.uv = input.uv;

    float3x3 normal_matrix = (float3x3)model;
    output.world_normal = normalize(mul(normal_matrix, input.normal));

    return output;
}
