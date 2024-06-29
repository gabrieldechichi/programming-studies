struct VertexIn {
    @location(0) pos: vec2f,
    @location(1) col: vec4f,
};

struct VertexOut {
    @builtin(position) pos: vec4f,
    @location(0) col: vec4f,
};

struct Uniforms {
    time: f32,
    color: vec4f,
}

@group(0) @binding(0) var<uniform> uniforms: Uniforms;

@vertex
fn vs_main(in: VertexIn) -> VertexOut {
    var out: VertexOut;

    let ratio = 800.0 / 600.0;
    var offset = vec2f(-0.6875, -0.463);
    offset += 0.3 * vec2f(cos(uniforms.time), sin(uniforms.time));
    out.pos = vec4f(in.pos.x + offset.x, (in.pos.y + offset.y) * ratio, 0.0, 1.0);
    //out.pos = vec4f(in.pos.x, in.pos.y, 0.0, 1.0);

    out.col = in.col * uniforms.color;
    return out;
}

@fragment
fn fs_main(in: VertexOut) -> @location(0) vec4f {
	// We apply a gamma-correction to the color
    let corrected_color = pow(in.col.rgb, vec3f(2.2));
    return vec4f(corrected_color, 1.0);
}
