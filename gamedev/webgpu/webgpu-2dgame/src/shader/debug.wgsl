struct VertexIn {
    @location(0) pos: vec2f,
    @location(1) instanceModelMatrixCol0: vec4<f32>,
    @location(2) instanceModelMatrixCol1: vec4<f32>,
    @location(3) instanceModelMatrixCol2: vec4<f32>,
    @location(4) instanceModelMatrixCol3: vec4<f32>,
    @location(5) instanceColor: vec4f,
}

struct VertexOut {
    @builtin(position) pos: vec4f,
    @location(0) color: vec4f,
}

@group(0) @binding(0)
var<uniform> viewProjectionMatrix: mat4x4f;

@vertex
fn vertexMain(in: VertexIn) -> VertexOut {
    var out: VertexOut;

    let modelMatrix = mat4x4<f32>(
        in.instanceModelMatrixCol0,
        in.instanceModelMatrixCol1,
        in.instanceModelMatrixCol2,
        in.instanceModelMatrixCol3,
    );
    out.pos = viewProjectionMatrix * modelMatrix * vec4(in.pos, 0.0, 1.0);
    out.color = in.instanceColor;

    return out;
}

@fragment
fn fragmentMain(in: VertexOut) -> @location(0) vec4f {
    return in.color;
}
