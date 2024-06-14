struct VertexIn {
    @builtin(vertex_index) vertexIndex: u32,
    @location(0) pos: vec2f,
}

struct VertexOut {
    @builtin(position) pos: vec4f,
    @location(0) color: vec4f,
}

struct Uniform {
    color: vec4<f32>,
}

@group(0) @binding(0)
var<uniform> viewProjectionMatrix: mat4x4f;

@group(0) @binding(1)
var<uniform> uniforms: Uniform;

@vertex
fn vertexMain(in: VertexIn) -> VertexOut {
    let pos = array(
        vec2f(0.0, 0.5),  // top center
        vec2f(-0.5, -0.5),  // bottom left
        vec2f(0.5, -0.5)   // bottom right
    );
    var out: VertexOut;
    out.pos = viewProjectionMatrix * vec4(in.pos, 0, 1);
    out.color = uniforms.color;

    //out.pos = vec4(pos[in.vertexIndex], 0.0, 1.0);
    return out;
}

@fragment
fn fragmentMain(in: VertexOut) -> @location(0) vec4f {
    return vec4(1.0, 0.0, 0.0, 1.0);
}
