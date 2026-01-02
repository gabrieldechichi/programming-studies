const LIGHT_DIR: vec3<f32> = vec3<f32>(0.5, 0.8, 0.3);
const AMBIENT: f32 = 0.15;

@fragment
fn fs_main(@location(0) world_normal: vec3<f32>, @location(1) material_color: vec4<f32>) -> @location(0) vec4<f32> {
    let light_dir = normalize(LIGHT_DIR);
    let n = normalize(world_normal);
    let ndotl = max(dot(n, light_dir), 0.0);
    let diffuse = AMBIENT + (1.0 - AMBIENT) * ndotl;
    return vec4<f32>(material_color.rgb * diffuse, material_color.a);
}
