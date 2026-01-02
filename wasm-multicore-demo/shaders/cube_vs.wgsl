struct GlobalUniforms {
    model: mat4x4<f32>,
    view: mat4x4<f32>,
    proj: mat4x4<f32>,
    view_proj: mat4x4<f32>,
};

@group(0) @binding(0) var<uniform> global: GlobalUniforms;
@group(0) @binding(1) var<uniform> color: vec4<f32>;

struct VertexInput {
    @location(0) position: vec3<f32>,
    @location(1) normal: vec3<f32>,
    @location(2) vertex_color: vec4<f32>,
};

struct VertexOutput {
    @builtin(position) position: vec4<f32>,
    @location(0) world_normal: vec3<f32>,
    @location(1) material_color: vec4<f32>,
};

@vertex
fn vs_main(in: VertexInput) -> VertexOutput {
    var out: VertexOutput;
    let mvp = global.view_proj * global.model;
    out.position = mvp * vec4<f32>(in.position, 1.0);
    let normal_matrix = mat3x3<f32>(global.model[0].xyz, global.model[1].xyz, global.model[2].xyz);
    out.world_normal = normalize(normal_matrix * in.normal);
    out.material_color = color;
    return out;
}
