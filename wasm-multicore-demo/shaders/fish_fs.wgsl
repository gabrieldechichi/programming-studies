#include "fog.wgsl"

@group(2) @binding(0) var albedo_sampler: sampler;
@group(2) @binding(1) var albedo_texture: texture_2d<f32>;
@group(2) @binding(2) var tint_sampler: sampler;
@group(2) @binding(3) var tint_texture: texture_2d<f32>;
@group(2) @binding(4) var metallic_gloss_sampler: sampler;
@group(2) @binding(5) var metallic_gloss_texture: texture_2d<f32>;

@fragment
fn fs_main(in: FragmentInput) -> @location(0) vec4<f32> {
    let albedo_sample = textureSample(albedo_texture, albedo_sampler, in.uv);
    let tint_uv = vec2<f32>(material.tint_offset, 0.0);
    let tint_sample = textureSample(tint_texture, tint_sampler, tint_uv).rgb;
    let metallic_gloss = textureSample(metallic_gloss_texture, metallic_gloss_sampler, in.uv);

    let tinted = albedo_sample.rgb * tint_sample;
    let base_color = mix(albedo_sample.rgb, tinted, albedo_sample.a) * material.tint_color.rgb;
    let metallic = material.metallic;
    let smoothness = metallic_gloss.a * material.smoothness;

    let final_color = pbr_lighting(base_color, metallic, smoothness, in.world_normal, in.world_position);
    return vec4<f32>(apply_fog(final_color, in.world_position), 1.0);
}
