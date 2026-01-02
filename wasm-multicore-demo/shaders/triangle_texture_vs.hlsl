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
    float2 uv : TEXCOORD1;
};

struct VertexOutput {
    float4 position : SV_POSITION;
    float2 uv : TEXCOORD0;
};

VertexOutput vs_main(VertexInput input) {
    VertexOutput output;
    float4x4 mvp = mul(view_proj, model);
    output.position = mul(mvp, float4(input.position, 1.0));
    output.uv = input.uv;
    return output;
}
