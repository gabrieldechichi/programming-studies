struct VertexIn {
    @location(0) pos: vec2f,
}

struct VertexOut {
    @builtin(position) pos: vec4f,
}

struct Uniforms {
    modelMatrix: mat4x4f,
    viewProjectionMatrix: mat4x4f,
};

@group(0) @binding(0) var<uniform> uniforms: Uniforms;

@vertex
fn vertexMain(in: VertexIn) -> VertexOut {
    var out: VertexOut;
    out.pos = uniforms.viewProjectionMatrix * uniforms.modelMatrix * vec4(in.pos, 0.0, 1.0);
    return out;
}

@fragment
fn fragmentMain(in: VertexOut) -> @location(0) vec4f {
    return vec4f(1, 0, 0, 1);
}
