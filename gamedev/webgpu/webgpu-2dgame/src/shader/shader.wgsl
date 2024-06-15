struct VertexIn {
    @location(0) pos: vec2f,
    @location(1) texCoords: vec2f,
    @location(2) instanceModelMatrixCol0: vec4<f32>,
    @location(3) instanceModelMatrixCol1: vec4<f32>,
    @location(4) instanceModelMatrixCol2: vec4<f32>,
    @location(5) instanceModelMatrixCol3: vec4<f32>,
    //normalized offset in the texture atlas for this instance
    @location(6) instanceTexCoordOffset: vec2f,
    //normalize scale (w,h) in the texture atlas for this instance
    @location(7) instanceTexCoordScale: vec2f,
}

struct VertexOut {
    @builtin(position) pos: vec4f,
    @location(0) texCoords: vec2f,
}

@group(0) @binding(0)
var<uniform> viewProjectionMatrix: mat4x4f;

@vertex
fn vertexMain(in: VertexIn) -> VertexOut {
    let modelMatrix = mat4x4<f32>(
        in.instanceModelMatrixCol0,
        in.instanceModelMatrixCol1,
        in.instanceModelMatrixCol2,
        in.instanceModelMatrixCol3,
    );

    var out: VertexOut;
    out.pos = viewProjectionMatrix * modelMatrix * vec4(in.pos, 0.0, 1.0);
    out.texCoords = in.texCoords * in.instanceTexCoordScale + in.instanceTexCoordOffset;
    //out.pos = vec4(in.pos, 0.0, 1.0);
    //out.texCoords = in.texCoords;
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
