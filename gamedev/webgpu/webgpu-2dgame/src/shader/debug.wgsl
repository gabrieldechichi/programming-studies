struct VertexIn {
    @location(0) pos: vec2f,
    @location(1) instancePos: vec2f,
    @location(2) instanceSize: vec2f,
    @location(3) instanceColor: vec4f,
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
    out.pos = viewProjectionMatrix * vec4(in.pos * in.instanceSize + in.instancePos, 0.0, 1.0);//this doesn't
    out.color = in.instanceColor;
    return out;
}

@fragment
fn fragmentMain(in: VertexOut) -> @location(0) vec4f {
    return in.color;
}
