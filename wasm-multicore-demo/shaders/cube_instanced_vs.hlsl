cbuffer GlobalUniforms : register(b0) {
    row_major float4x4 model;
    row_major float4x4 view;
    row_major float4x4 proj;
    row_major float4x4 view_proj;
};

cbuffer MaterialUniforms : register(b1) {
    float4 color;
};

ByteAddressBuffer instances : register(t0);

struct VertexInput {
    float3 position : POSITION;
    float3 normal : NORMAL;
    float4 vertex_color : COLOR;
    uint instance_id : SV_InstanceID;
};

struct VertexOutput {
    float4 position : SV_POSITION;
    float3 world_normal : TEXCOORD0;
    float4 material_color : TEXCOORD1;
};

VertexOutput vs_main(VertexInput input) {
    VertexOutput output;

    float4x4 instance_model = asfloat(uint4x4(
        instances.Load4(input.instance_id * 64 + 0),
        instances.Load4(input.instance_id * 64 + 16),
        instances.Load4(input.instance_id * 64 + 32),
        instances.Load4(input.instance_id * 64 + 48)
    ));

    float4 world_pos = mul(float4(input.position, 1.0), instance_model);
    output.position = mul(world_pos, view_proj);

    float3x3 normal_matrix = (float3x3)instance_model;
    output.world_normal = normalize(mul(input.normal, normal_matrix));

    output.material_color = color;

    return output;
}
