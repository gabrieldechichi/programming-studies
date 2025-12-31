struct GlobalUniforms {
    model: mat4x4<f32>,
    view: mat4x4<f32>,
    proj: mat4x4<f32>,
    view_proj: mat4x4<f32>,
    camera_pos: vec3<f32>,
    time: f32,
};

@group(0) @binding(0) var<uniform> global: GlobalUniforms;

struct VertexInput {
    @location(0) position: vec3<f32>,
    @location(1) normal: vec3<f32>,
    @location(2) tangent: vec4<f32>,
    @location(3) uv: vec2<f32>,
};

struct VertexOutput {
    @builtin(position) position: vec4<f32>,
    @location(0) uv: vec2<f32>,
    @location(1) world_normal: vec3<f32>,
    @location(2) world_position: vec3<f32>,
};

struct FragmentInput {
    @location(0) uv: vec2<f32>,
    @location(1) world_normal: vec3<f32>,
    @location(2) world_position: vec3<f32>,
};

const LIGHT_DIR: vec3<f32> = vec3<f32>(0, 0.901, 0.433);
const LIGHT_COLOR: vec3<f32> = vec3<f32>( 169.0f, 248.0f, 255.0f) * (2.0f / 255f);
const AMBIENT: vec3<f32> = vec3<f32>(1.0) * 0.25;
const DIELECTRIC_F0: f32 = 0.04;

fn pbr_lighting(
    base_color: vec3<f32>,
    metallic: f32,
    smoothness: f32,
    world_normal: vec3<f32>,
    world_position: vec3<f32>
) -> vec3<f32> {
    let perceptual_roughness = 1.0 - smoothness;
    let roughness = perceptual_roughness * perceptual_roughness;
    let roughness2 = roughness * roughness;

    let one_minus_reflectivity = (1.0 - DIELECTRIC_F0) * (1.0 - metallic);
    let diffuse_color = base_color * one_minus_reflectivity;
    let specular_color = mix(vec3<f32>(DIELECTRIC_F0), base_color, metallic);

    let N = normalize(world_normal);
    let V = normalize(global.camera_pos - world_position);
    let L = normalize(LIGHT_DIR);
    let H = normalize(L + V);

    let NdotL = max(dot(N, L), 0.0);
    let NdotV = max(dot(N, V), 0.0001);
    let NdotH = max(dot(N, H), 0.0);
    let LdotH = max(dot(L, H), 0.0);

    let d = NdotH * NdotH * (roughness2 - 1.0) + 1.00001;
    let LoH2 = LdotH * LdotH;
    let normalization = roughness * 4.0 + 2.0;
    let specular_term = roughness2 / ((d * d) * max(0.1, LoH2) * normalization);

    let radiance = LIGHT_COLOR * NdotL;
    let direct = (diffuse_color + specular_term * specular_color) * radiance;
    let ambient = AMBIENT * diffuse_color;

    return ambient + direct;
}
