struct GlobalUniforms {
    model: mat4x4<f32>,
    view: mat4x4<f32>,
    proj: mat4x4<f32>,
    view_proj: mat4x4<f32>,
};

struct InstanceData {
    model: mat4x4<f32>,
};

@group(0) @binding(0) var<uniform> global: GlobalUniforms;
@group(0) @binding(1) var<uniform> color: vec4<f32>;
@group(1) @binding(0) var<storage, read> instances: array<InstanceData>;

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
fn vs_main(@builtin(instance_index) instance_idx: u32, in: VertexInput) -> VertexOutput {
    var out: VertexOutput;
    let model = instances[instance_idx].model;
    let mvp = global.view_proj * model;
    out.position = mvp * vec4<f32>(in.position, 1.0);
    let normal_matrix = mat3x3<f32>(model[0].xyz, model[1].xyz, model[2].xyz);
    out.world_normal = normalize(normal_matrix * in.normal);
    out.material_color = color;
    return out;
}
