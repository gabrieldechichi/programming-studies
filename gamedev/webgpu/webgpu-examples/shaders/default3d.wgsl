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

    let angle = uniforms.time;
    //let angle = 0.8;
    let alpha = cos(angle);
    let beta = sin(angle);
    var pos = vec3f(
        in.pos.x,
        alpha * in.pos.y + beta * in.pos.z,
        alpha * in.pos.z - beta * in.pos.y,
    );

    out.pos = vec4f(pos.x, pos.y, pos.z, 1.0);
    out.col = in.col;
    return out;
}

@fragment
fn fs_main(in: VertexOut) -> @location(0) vec4f {
	// We apply a gamma-correction to the color
    let corrected_color = pow(in.col.rgb, vec3f(2.2));
    return vec4f(corrected_color, 1.0);
}
