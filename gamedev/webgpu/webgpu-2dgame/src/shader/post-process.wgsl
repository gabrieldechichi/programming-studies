struct VertexIn {
    @location(0) pos: vec2f,
    @location(1) texCoords: vec2f,
}

struct VertexOut {
    @builtin(position) pos: vec4f,
    @location(0) texCoords: vec2f,
}

@vertex
fn vertexMain(in: VertexIn) -> VertexOut {
    var out: VertexOut;
    out.pos = vec4(in.pos, 0, 1);
    out.texCoords = in.texCoords;
    return out;
}

@group(0) @binding(0)
var texSampler: sampler;
@group(0) @binding(1)
var tex: texture_2d<f32>;

@fragment
fn fragmentMain(in: VertexOut) -> @location(0) vec4f {
    var texColor = textureSample(tex, texSampler, in.texCoords);
    let avg = (texColor.r + texColor.g + texColor.b) / 3;
    return vec4(avg, avg, avg, 1.0);
}
