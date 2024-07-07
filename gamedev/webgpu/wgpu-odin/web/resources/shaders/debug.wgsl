struct VertexIn {
    @builtin(instance_index) instanceIdx: u32,
    @location(0) pos: vec2f,
}

struct VertexOut {
    @builtin(position) pos: vec4f,
}

@group(0) @binding(0) var<uniform> viewProjectionMatrix: mat4x4f;
@group(0) @binding(1) var<uniform> modelMatrices: array<mat4x4f, 1024>;

@vertex
fn vertexMain(in: VertexIn) -> VertexOut {
    var out: VertexOut;
    let modelMatrix: mat4x4f = modelMatrices[in.instanceIdx];
    out.pos = viewProjectionMatrix * modelMatrix * vec4(in.pos, 0.0, 1.0);
    return out;
}

@fragment
fn fragmentMain(in: VertexOut) -> @location(0) vec4f {
    return vec4f(1, 0, 0, 1);
}
