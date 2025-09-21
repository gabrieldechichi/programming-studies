#version 450

// Uniform buffers (matching renderer.h structures)
layout(binding = 0) uniform camera_params {
    mat4 view_matrix;
    mat4 projection_matrix;
    vec3 camera_pos;
    float _padding1;
} camera;

// Model matrix via push constants (for simplicity and compatibility)
layout(push_constant) uniform Uniforms {
    mat4 model;
} uniforms;

// Input vertex attributes
layout(location = 0) in vec2 inPosition;
layout(location = 1) in vec4 inColor;

// Output to fragment shader
layout(location = 0) out vec4 fragColor;
layout(location = 1) out vec3 vs_world_pos;
layout(location = 2) out vec3 vs_view_pos;

void main() {
    // Expand 2D position to 3D (z = 0)
    vec4 position = vec4(inPosition, 0.0, 1.0);

    // Transform to world space
    vec4 worldPos = uniforms.model * position;
    vs_world_pos = worldPos.xyz;

    // Transform to view space
    vec4 viewPos = camera.view_matrix * worldPos;
    vs_view_pos = viewPos.xyz;

    // Transform to clip space
    gl_Position = camera.projection_matrix * viewPos;

    // Pass color to fragment shader
    fragColor = inColor;
}