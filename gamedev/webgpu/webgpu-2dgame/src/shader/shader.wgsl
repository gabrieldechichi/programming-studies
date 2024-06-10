struct VertexIn {
    @location(0) position: vec2f,
    @location(1) texCoords: vec2f,
    @location(2) color: vec4f,
    @location(3) modelMatrixRow1: vec4f,
    @location(4) modelMatrixRow2: vec4f,
    @location(5) modelMatrixRow3: vec4f,
    @location(6) modelMatrixRow4: vec4f,
}

struct VertexOut {
    @builtin(position) pos: vec4f,
    @location(0) texCoords: vec2f,
    @location(1) color: vec4f,
}

@group(0) @binding(0)
var<uniform> viewProjectionMatrix: mat4x4f;

@vertex
fn vertexMain(in: VertexIn) -> VertexOut {
    let modelMatrix = mat4x4<f32>(
        in.modelMatrixRow1,
        in.modelMatrixRow2,
        in.modelMatrixRow3,
        in.modelMatrixRow4
    );

    var out: VertexOut;
    out.pos = viewProjectionMatrix * modelMatrix * vec4(in.position, 0.0, 1.0);
    out.texCoords = in.texCoords;
    out.color = in.color;
    return out;
}

@group(1) @binding(0)
var texSampler: sampler;

@group(1) @binding(1)
var tex: texture_2d<f32>;

@fragment
fn fragmentMain(in: VertexOut) -> @location(0) vec4f {
    var texColor = textureSample(tex, texSampler, in.texCoords);
    return in.color * texColor;
}
