struct VertexIn {
    @builtin(instance_index) instanceIdx: u32,
    @location(0) pos: vec2f,
}

struct VertexOut {
    @builtin(position) pos: vec4f,
}

struct Uniforms {
    viewProjectionMatrix: mat4x4f,
    modelMatrices: array<mat4x4f, 256>,
};

@group(0) @binding(0) var<uniform> uniforms: Uniforms;

@vertex
fn vertexMain(in: VertexIn) -> VertexOut {
    var out: VertexOut;
    let modelMatrix: mat4x4f = uniforms.modelMatrices[in.instanceIdx];
    out.pos = uniforms.viewProjectionMatrix * modelMatrix * vec4(in.pos, 0.0, 1.0);
    return out;
}

@fragment
fn fragmentMain(in: VertexOut) -> @location(0) vec4f {
    return vec4f(1, 0, 0, 1);
}
