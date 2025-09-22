#version 450

// Uniform buffers
layout(binding = 3) uniform fs_params {
    vec4 uColor;
} material;

#define MAX_DIRECTIONAL_LIGHTS 4

layout(binding = 4) uniform light_params {
    float count;
    float _pad0;
    float _pad1;
    float _pad2;
    vec4 lights_data[MAX_DIRECTIONAL_LIGHTS * 2]; // Each light uses 2 vec4s
} directional_lights;

// Textures and samplers (using combined image samplers)
layout(binding = 5) uniform sampler2D uTexture;
layout(binding = 8) uniform sampler2D uDetailTexture;

// Inputs from vertex shader
layout(location = 0) in vec3 vs_normal;
layout(location = 1) in vec2 vs_texcoord;
layout(location = 2) in vec3 vs_world_pos;
layout(location = 3) in vec3 vs_eye_dir;

// Output
layout(location = 0) out vec4 frag_color;

// Helper functions
#define saturate(a) (clamp(a, 0.0, 1.0))
#define lerp(a, b, t) (mix(a, b, t))

vec3 anime_light_pass(vec3 color, vec3 normal, vec3 worldPos) {
    vec3 light_contribution = vec3(0.0);

    int num_lights = int(directional_lights.count);
    for (int i = 0; i < num_lights && i < MAX_DIRECTIONAL_LIGHTS; i++) {
        vec4 direction_data = directional_lights.lights_data[i * 2 + 0];
        vec4 color_data = directional_lights.lights_data[i * 2 + 1];

        vec3 light_direction = direction_data.xyz;
        vec3 light_color = color_data.xyz;
        float light_intensity = color_data.w;

        float diffuse = dot(normalize(light_direction), normal);
        diffuse = max(diffuse, 0.0);

        // Toon shading step function
        vec3 d = smoothstep(vec3(0), vec3(0.01), vec3(diffuse));
        d = lerp(vec3(0.5), light_color * light_intensity * 0.719, d);
        light_contribution += d;
    }

    // Add ambient light
    float ambient_intensity = 0.45;
    vec3 ambient_color = vec3(1.0, 1.0, 1.0);
    light_contribution += ambient_color * ambient_intensity;

    return light_contribution;
}

void main() {
    // Sample the main texture
    vec4 tex_color = texture(uTexture, vs_texcoord);
    frag_color = tex_color;
    return;

    // Sample detail texture and mix with main texture
    vec4 detail_color = texture(uDetailTexture, vs_texcoord);
    tex_color.rgb = mix(tex_color.rgb, detail_color.rgb, detail_color.a);

    // Normalize the interpolated normal
    vec3 normal = normalize(vs_normal);

    // Start with white base color for lighting
    vec3 color = vec3(1.0);

    // Apply anime/toon lighting
    color = anime_light_pass(color, normal, vs_world_pos);

    // Multiply by texture color
    color *= tex_color.rgb;

    // Apply material color tint
    color *= material.uColor.rgb;

    // Output final color with alpha from texture and material
    frag_color = vec4(color, tex_color.a * material.uColor.a);
}