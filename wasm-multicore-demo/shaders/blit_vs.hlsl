struct VertexOutput {
    float4 position : SV_POSITION;
    float2 uv : TEXCOORD0;
};

VertexOutput vs_main(uint vertexId : SV_VertexID) {
    VertexOutput output;

    // Fullscreen triangle: 3 vertices cover the screen
    // Vertex 0: (-1, -1), Vertex 1: (3, -1), Vertex 2: (-1, 3)
    float2 pos = float2((vertexId << 1) & 2, vertexId & 2);
    pos = pos * 2.0 - 1.0;

    output.position = float4(pos.x, -pos.y, 0.0, 1.0);
    output.uv = float2((vertexId << 1) & 2, vertexId & 2) * 0.5;

    return output;
}
