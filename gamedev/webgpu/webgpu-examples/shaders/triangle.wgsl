struct VertexIn {
    @builtin(vertex_index) index: u32,
};

struct VertexOut {
    @builtin(position) pos: vec4f,
};

@vertex
fn vs_main(in: VertexIn) -> VertexOut {
    var out : VertexOut;
    var p = vec2f(0.0, 0.0);
    if in.index == 0u {
        p = vec2f(-0.5, -0.5);
    } else if in.index == 1u {
        p = vec2f(0.5, -0.5);
    } else {
        p = vec2f(0.0, 0.5);
    }
    out.pos = vec4f(p, 0.0, 1.0);
    return out;
}

@fragment
fn fs_main() -> @location(0) vec4f {
    return vec4f(0.0, 0.4, 1.0, 1.0);
}
