struct VertexIn {
    @location(0) pos: vec2f,
    @location(1) col: vec4f,
};

struct VertexOut {
    @builtin(position) pos: vec4f,
    @location(0) col: vec4f,
};

@vertex
fn vs_main(in: VertexIn) -> VertexOut {
    var out: VertexOut;
    out.pos = vec4f(in.pos, 0.0, 1.0);

    out.col = vec4f(0.0, 0.4, 1.0, 1.0);
    out.col = in.col;
    return out;
}

@fragment
fn fs_main(in: VertexOut) -> @location(0) vec4f {
    return in.col;
}
