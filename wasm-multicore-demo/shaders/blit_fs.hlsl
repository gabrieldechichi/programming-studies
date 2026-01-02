Texture2D blitTexture : register(t0);
SamplerState blitSampler : register(s0);

struct VertexOutput {
    float4 position : SV_POSITION;
    float2 uv : TEXCOORD0;
};

float4 ps_main(VertexOutput input) : SV_TARGET {
    float3 color = blitTexture.Sample(blitSampler, input.uv).rgb;

    // Simple tonemapping: clamp to [0,1]
    color = saturate(color);

    return float4(color, 1.0);
}
