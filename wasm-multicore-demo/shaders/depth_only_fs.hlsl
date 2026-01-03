struct DepthPixelInput {
    float4 position : SV_POSITION;
};

void ps_main(DepthPixelInput input) {
    // Depth-only pass - no color output
}
