@group(2) @binding(0) var tex_sampler: sampler;
@group(2) @binding(1) var tex: texture_2d<f32>;

struct FragmentInput {
    @location(0) uv: vec2<f32>,
};

@fragment
fn fs_main(in: FragmentInput) -> @location(0) vec4<f32> {
    return textureSample(tex, tex_sampler, in.uv);
}
