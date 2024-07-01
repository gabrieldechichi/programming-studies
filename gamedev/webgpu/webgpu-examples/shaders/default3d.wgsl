struct VertexIn {
    @location(0) pos: vec3f,
    @location(1) col: vec4f,
};

struct VertexOut {
    @builtin(position) pos: vec4f,
    @location(0) col: vec4f,
};

struct Uniforms {
    time: f32,
};

@group(0) @binding(0) var<uniform> uniforms: Uniforms;

@vertex
fn vs_main(in: VertexIn) -> VertexOut {
    var out: VertexOut;

    // Rotate the model in the XY plane
    let angley = uniforms.time;
    let rot1 = transpose(mat4x4f(
        cos(angley), 0.0, sin(angley), 0.0,
        0.0, 1.0, 0.0, 0.0,
        -sin(angley), 0.0, cos(angley), 0.0,
        0.0, 0.0, 0.0, 1.0,
    ));

    let anglex = -1.0 * 3.14 / 4.0;
    let rot2 = transpose(mat4x4f(
        1.0, 0.0, 0.0, 0.0,
        0.0, cos(anglex), sin(anglex), 0.0,
        0.0, -sin(anglex), cos(anglex), 0.0,
        0.0, 0.0, 0.0, 1.0,
    ));

    let R = rot2 * rot1;

    let focalPoint = vec3f(0.0, 0.0, -2.0);
    let T = transpose(mat4x4f(
        1.0, 0.0, 0.0, -focalPoint.x,
        0.0, 1.0, 0.0, -focalPoint.y,
        0.0, 0.0, 1.0, -focalPoint.z,
        0.0, 0.0, 0.0, 1.0,
    ));

    let S = transpose(mat4x4f(
        0.5, 0.0, 0.0, 0.0,
        0.0, 0.6, 0.0, 0.0,
        0.0, 0.0, 0.5, 0.0,
        0.0, 0.0, 0.0, 1.0,
    ));

    //let near = -1.0;
    //let far = 1.0;
    //let scale = 0.7;
    //let P = transpose(mat4x4f(
    //    1.0 / scale, 0.0, 0.0, 0.0,
    //    0.0, 1.0 / scale, 0.0, 0.0,
    //    0.0, 0.0, 1.0 / (far - near), -near / (far - near),
    //    0.0, 0.0, 0.0, 1.0,
    //));

    let focalLength = 2.0;
    let near = 0.01;
    let far = 100.0;
    let divides = 1.0 / (focalLength * (far - near));
// (no need for a scale parameter now that we have focalLength)
    let P = transpose(mat4x4f(
        focalLength, 0.0, 0.0, 0.0,
        0.0, focalLength, 0.0, 0.0,
        0.0, 0.0, far * divides, -far * near * divides,
        0.0, 0.0, 1.0 / focalLength, 0.0,
    ));

    out.pos = P * T * R * S * vec4f(in.pos, 1.0);
    out.col = in.col;
    return out;
}

@fragment
fn fs_main(in: VertexOut) -> @location(0) vec4f {
	// We apply a gamma-correction to the color
    let corrected_color = pow(in.col.rgb, vec3f(2.2));
    return vec4f(corrected_color, 1.0);
}
