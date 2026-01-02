struct VertexOutput {
    float4 position : SV_POSITION;
};

float4 ps_main(VertexOutput input) : SV_TARGET {
    return float4(1.0, 0.0, 0.0, 1.0);
}
