struct VertexIn {
    @location(0) pos: vec3f,
    @location(1) col: vec4f,
};

struct VertexOut {
    @builtin(position) pos: vec4f,
    @location(0) col: vec4f,
};

@vertex
fn vs_main(in: VertexIn) -> VertexOut {
    var out: VertexOut;

    out.pos = vec4f(in.pos.x, in.pos.y, in.pos.z, 1.0);
    out.col = in.col;
    return out;
}

@fragment
fn fs_main(in: VertexOut) -> @location(0) vec4f {
	// We apply a gamma-correction to the color
    let corrected_color = pow(in.col.rgb, vec3f(2.2));
    return vec4f(corrected_color, 1.0);
}
