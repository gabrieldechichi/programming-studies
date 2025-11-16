#version 450

// Vertex attributes
layout(location = 0) in vec3 aPosition;
layout(location = 1) in vec2 aTexCoord;

// Uniform buffers
layout(binding = 0, std140) uniform CameraParams {
    vec3 camera_pos;
    float _pad0;
    mat4 view_matrix;
    mat4 projection_matrix;
    mat4 view_proj_matrix;
} uCamera;

layout(binding = 1, std140) uniform ModelParams {
    mat4 model;
} uModel;

// Outputs to fragment shader
layout(location = 0) out vec2 vTexCoord;

void main() {
    gl_Position = vec4(aPosition.xy, 0.0, 1.0);

    // vec4 worldPos = uModel.model * vec4(aPosition, 1.0);
    // gl_Position = uCamera.view_proj_matrix * worldPos;

    // Pass through texture coordinates
    vTexCoord = aTexCoord;
}
