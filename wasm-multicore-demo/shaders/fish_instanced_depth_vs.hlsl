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

ByteAddressBuffer instances : register(t0);

struct DepthVertexInput {
    float3 position : TEXCOORD0;
    float3 normal : TEXCOORD1;
    float4 tangent : TEXCOORD2;
    float2 uv : TEXCOORD3;
    uint instance_id : SV_InstanceID;
};

struct DepthVertexOutput {
    float4 position : SV_POSITION;
};

DepthVertexOutput vs_main(DepthVertexInput input) {
    DepthVertexOutput output;

    float4x4 instance_model = asfloat(uint4x4(
        instances.Load4(input.instance_id * 64 + 0),
        instances.Load4(input.instance_id * 64 + 16),
        instances.Load4(input.instance_id * 64 + 32),
        instances.Load4(input.instance_id * 64 + 48)
    ));

    float wiggle_input = (time + wave_offset) * wave_speed + input.position.y * wave_frequency;
    float x_offset = sin(wiggle_input) * wave_distance;
    float3 wiggled_pos = float3(input.position.x + x_offset, input.position.y, input.position.z);

    float4 world_pos = mul(float4(wiggled_pos, 1.0), instance_model);
    output.position = mul(world_pos, view_proj);

    return output;
}
