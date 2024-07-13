struct VertexIn {
    @location(0) pos: vec3f,
    @location(1) normal: vec3f,
    @location(2) col: vec4f,
};

struct VertexOut {
    @builtin(position) pos: vec4f,
    @location(0) normal: vec3f,
    @location(1) col: vec4f,
};

struct Uniforms {
    modelMatrix: mat4x4f,
    viewMatrix: mat4x4f,
    projectionMatrix: mat4x4f,
    time: f32,
};

@group(0) @binding(0) var<uniform> uniforms: Uniforms;

@vertex
fn vs_main(in: VertexIn) -> VertexOut {
    var out: VertexOut;

    out.pos = uniforms.projectionMatrix * uniforms.viewMatrix * uniforms.modelMatrix * vec4f(in.pos, 1.0);
    out.col = in.col;
    out.col = vec4f(1, 1, 1, 1);
    out.normal = (uniforms.modelMatrix * vec4f(in.normal, 0.0)).xyz;
    return out;
}

@fragment
fn fs_main(in: VertexOut) -> @location(0) vec4f {
	// We apply a gamma-correction to the color
    let lightColor1 = vec3f(1.0, 0.9, 0.6);
    let lightColor2 = vec3f(0.6, 0.9, 1.0);

    var color = in.col.rgb;
    let normal = normalize(in.normal);
    let shading = max(0.0, dot(lightColor1, normal));

    color *= shading;

    //let shading = 0.5 * in.normal.x - 0.9 * in.normal.y + 0.1 * in.normal.z;
    //let color = in.col.rgb * shading;
    //var corrected_color = pow(in.col.rgb, vec3f(2.2));
    //corrected_color *= dir_light * in.normal;
    return vec4f(color, 1.0);
}
