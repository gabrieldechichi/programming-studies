SamplerState samp : register(s0);
Texture2D tex : register(t1);

struct VertexOutput {
    float4 position : SV_POSITION;
    float2 uv : TEXCOORD0;
};

float4 ps_main(VertexOutput input) : SV_TARGET {
    return tex.Sample(samp, input.uv);
}
