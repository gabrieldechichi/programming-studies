#include <metal_stdlib>
#include <simd/simd.h>

using namespace metal;

struct VertexIn {
    float2 position [[attribute(0)]];
    float4 color    [[attribute(1)]];
};

struct VertexOut {
    float4 position [[position]];
    float4 color;
};

struct Uniforms {
    float4x4 model;
};

vertex VertexOut vertex_main(VertexIn in [[stage_in]],
                              constant Uniforms& uniforms [[buffer(1)]]) {
    VertexOut out;
    // Apply rotation transform
    float4 pos = uniforms.model * float4(in.position, 0.0, 1.0);
    pos.z = 0.5; // Set z to middle of depth range
    out.position = pos;
    out.color = in.color;
    return out;
}

fragment float4 fragment_main(VertexOut in [[stage_in]]) {
    // Return the interpolated color
    return in.color;
}