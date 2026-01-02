cbuffer Uniforms : register(b0) {
    float4x4 model;
};

struct VertexInput {
    float3 position : POSITION;
};

struct VertexOutput {
    float4 position : SV_POSITION;
};

VertexOutput vs_main(VertexInput input) {
    VertexOutput output;
    output.position = mul(model, float4(input.position, 1.0));
    return output;
}
