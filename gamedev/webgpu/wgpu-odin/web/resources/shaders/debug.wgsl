struct VertexIn {
    @builtin(instance_index) instanceIdx: u32,
    @location(0) pos: vec2f,
}

struct VertexOut {
    @builtin(position) pos: vec4f,
    @location(0) col: vec4f,
}

struct Globals {
    viewProjectionMatrix: mat4x4f,
    colors: array<vec4f, 4>,
}

@group(0) @binding(0) var<uniform> globals: Globals;
@group(0) @binding(1) var<uniform> modelMatrices: array<mat4x4f, 1024>;

@vertex
fn vertexMain(in: VertexIn) -> VertexOut {
    var out: VertexOut;
    let modelMatrix: mat4x4f = modelMatrices[in.instanceIdx];
    out.pos = globals.viewProjectionMatrix * modelMatrix * vec4(in.pos, 0.0, 1.0);
    out.col = globals.colors[in.instanceIdx % 4];
    return out;
}

@fragment
fn fragmentMain(in: VertexOut) -> @location(0) vec4f {
    return in.col;
}
