struct VertexIn {
    @location(0) position: vec2f,
    @location(1) texCoords: vec2f,
}

struct VertexOut {
    @builtin(position) pos: vec4f,
    @location(0) texCoords: vec2f,
}

@group(0) @binding(0)
var<uniform> viewProjectionMatrix: mat4x4f;

@vertex
fn vertexMain(in: VertexIn) -> VertexOut {
    var out: VertexOut;
    out.pos = viewProjectionMatrix * vec4(in.position, 0.0, 1.0);
    out.texCoords = in.texCoords;
    return out;
}

@group(1) @binding(0)
var texSampler: sampler;

@group(1) @binding(1)
var tex: texture_2d<f32>;

@fragment
fn fragmentMain(in: VertexOut) -> @location(0) vec4f {
    var texColor = textureSample(tex, texSampler, in.texCoords);
    return texColor;
}
