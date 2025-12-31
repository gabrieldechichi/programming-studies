#include "common.wgsl"

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

@vertex
fn vs_main(in: VertexInput) -> VertexOutput {
    var out: VertexOutput;
    let wiggle_input = (global.time + material.wave_offset) * material.wave_speed + in.position.y * material.wave_frequency;
    let x_offset = sin(wiggle_input) * material.wave_distance;
    let wiggled_pos = vec3<f32>(in.position.x + x_offset, in.position.y, in.position.z);
    let world_pos = global.model * vec4<f32>(wiggled_pos, 1.0);
    out.position = global.view_proj * world_pos;
    out.world_position = world_pos.xyz;
    out.uv = in.uv;
    let normal_matrix = mat3x3<f32>(global.model[0].xyz, global.model[1].xyz, global.model[2].xyz);
    out.world_normal = normalize(normal_matrix * in.normal);
    return out;
}
