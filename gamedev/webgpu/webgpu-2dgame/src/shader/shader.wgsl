struct VertexIn {
    @location(0) position: vec2f,
    @location(1) texCoords: vec2f,
    @location(2) color: vec4f,
}

struct VertexOut {
    @builtin(position) pos: vec4f,
    @location(0) texCoords: vec2f,
    @location(1) color: vec4f,
}

@vertex
fn vertexMain(in: VertexIn) -> VertexOut {
    var out: VertexOut;
    out.pos = vec4(in.position, 0.0, 1.0);
    out.texCoords = in.texCoords;
    out.color = in.color;
    return out;
}

@group(0) @binding(0)
var texSampler: sampler;

@group(0) @binding(1)
var tex: texture_2d<f32>;

@fragment
fn fragmentMain(in: VertexOut) -> @location(0) vec4f {
    var texColor = textureSample(tex, texSampler, in.texCoords);
    return in.color * texColor;
}
