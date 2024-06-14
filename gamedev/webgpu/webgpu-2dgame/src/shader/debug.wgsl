struct VertexIn {
    @location(0) pos: vec2f,
    @location(1) instancePos: vec2f,
    @location(2) instanceRot: f32,
    @location(3) instanceSize: vec2f,
    @location(4) instanceColor: vec4f,
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

    let rot: f32 = in.instanceRot;
    let origin = in.instancePos;

    // Calculate the cosine and sine of the rotation angle
    let cosTheta = cos(rot);
    let sinTheta = sin(rot);

    // Translate the vertex position relative to the instance's origin
    var translatedPos = (in.pos * in.instanceSize);

    // Rotate the translated position
    var rotatedPos = vec2<f32>(
        translatedPos.x * cosTheta - translatedPos.y * sinTheta,
        translatedPos.x * sinTheta + translatedPos.y * cosTheta
    );

    // Translate the rotated position to the instance's position
    var finalPos = rotatedPos + origin;

    // Transform the position using the viewProjectionMatrix
    out.pos = viewProjectionMatrix * vec4(finalPos, 0.0, 1.0);

    // Pass the instance color to the fragment shader
    out.color = in.instanceColor;

    return out;
}

@fragment
fn fragmentMain(in: VertexOut) -> @location(0) vec4f {
    return in.color;
}
