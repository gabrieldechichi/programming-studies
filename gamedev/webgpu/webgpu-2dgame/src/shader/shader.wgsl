struct VertexOut {
    @builtin(position) pos: vec4f,
    @location(0) color: vec4f,
}

@vertex
fn vertexMain(@location(0) position: vec2f, @location(1) color: vec4f) -> VertexOut {
    var out : VertexOut;
    out.pos = vec4(position, 0.0, 1.0);
    out.color = color;
    return out;
}

@fragment
fn fragmentMain(in : VertexOut) -> @location(0) vec4f {
    return in.color;
}
