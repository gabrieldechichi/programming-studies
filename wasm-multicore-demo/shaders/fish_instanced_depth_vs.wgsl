#include "common.wgsl"

struct InstanceData {
    model: mat4x4<f32>,
};

struct MaterialUniforms {
    tint_color: vec4<f32>,
    tint_offset: f32,
    metallic: f32,
    smoothness: f32,
    wave_frequency: f32,
    wave_speed: f32,
    wave_distance: f32,
    wave_offset: f32,
};

@group(0) @binding(1) var<uniform> material: MaterialUniforms;
@group(1) @binding(0) var<storage, read> instances: array<InstanceData>;

struct DepthVertexOutput {
    @builtin(position) position: vec4<f32>,
};

@vertex
fn vs_main(@builtin(instance_index) instance_idx: u32, in: VertexInput) -> DepthVertexOutput {
    var out: DepthVertexOutput;
    let model = instances[instance_idx].model;
    let wiggle_input = (global.time + material.wave_offset) * material.wave_speed + in.position.y * material.wave_frequency;
    let x_offset = sin(wiggle_input) * material.wave_distance;
    let wiggled_pos = vec3<f32>(in.position.x + x_offset, in.position.y, in.position.z);
    let world_pos = model * vec4<f32>(wiggled_pos, 1.0);
    out.position = global.view_proj * world_pos;
    return out;
}
