struct VertexOut {
    @builtin(position) pos: vec4f,
    @location(0) color: vec4f,
}

@vertex
fn vertexMain(@builtin(vertex_index) vertexIndex: u32) -> VertexOut {
    let pos = array(
        vec2(-0.5, -0.5),//bottom left
        vec2(0.5, -0.5),//bottom right
        vec2(0.0, 0.5),//top
    );
    let colors = array(
        vec4(1.0, 0.0, 0.0, 1.0),
        vec4(0.0, 1.0, 0.0, 1.0),
        vec4(0.0, 0.0, 1.0, 1.0),
    );

    var out : VertexOut;
    out.pos = vec4(pos[vertexIndex], 0.0, 1.0);
    out.color = colors[vertexIndex];
    return out;
}

@fragment
fn fragmentMain(in : VertexOut) -> @location(0) vec4f {
    return in.color;
}
