cbuffer GlobalUniforms : register(b0) {
    float4x4 model;
    float4x4 view;
    float4x4 proj;
    float4x4 view_proj;
};

cbuffer MaterialUniforms : register(b1) {
    float4 color;
};

struct VertexInput {
    float3 position : POSITION;
    float3 normal : NORMAL;
    float4 vertex_color : COLOR;
};

struct VertexOutput {
    float4 position : SV_POSITION;
    float3 world_normal : TEXCOORD0;
    float4 material_color : TEXCOORD1;
};

VertexOutput vs_main(VertexInput input) {
    VertexOutput output;

    float4x4 mvp = mul(view_proj, model);
    output.position = mul(mvp, float4(input.position, 1.0));

    float3x3 normal_matrix = (float3x3)model;
    output.world_normal = normalize(mul(normal_matrix, input.normal));

    output.material_color = color;

    return output;
}
