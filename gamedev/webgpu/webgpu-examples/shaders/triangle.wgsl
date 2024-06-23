struct VertexIn {
    @location(0) pos: vec2f,
};

struct VertexOut {
    @builtin(position) pos: vec4f,
};

@vertex
fn vs_main(in: VertexIn) -> VertexOut {
    var out: VertexOut;
    out.pos = vec4f(in.pos, 0.0, 1.0);
    return out;
}

@fragment
fn fs_main() -> @location(0) vec4f {
    return vec4f(0.0, 0.4, 1.0, 1.0);
}
