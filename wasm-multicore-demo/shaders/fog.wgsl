const FOG_COLOR: vec3<f32> = vec3<f32>(2.0 / 255.0, 94.0 / 255.0, 131.0 / 255.0);
const FOG_DENSITY: f32 = 0.007;

fn fog_linear(distance: f32, start: f32, end: f32) -> f32 {
    return clamp((end - distance) / (end - start), 0.0, 1.0);
}

fn fog_exp(distance: f32, density: f32) -> f32 {
    return clamp(exp(-density * distance), 0.0, 1.0);
}

fn fog_exp2(distance: f32, density: f32) -> f32 {
    let d = density * distance;
    return clamp(exp(-d * d), 0.0, 1.0);
}

fn apply_fog(color: vec3<f32>, world_position: vec3<f32>) -> vec3<f32> {
    let distance = length(global.camera_pos - world_position);
    let fog_factor = fog_exp2(distance, FOG_DENSITY);
    return mix(FOG_COLOR, color, fog_factor);
}
