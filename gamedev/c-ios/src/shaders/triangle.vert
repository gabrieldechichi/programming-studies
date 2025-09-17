#version 450

// Input vertex attributes
layout(location = 0) in vec2 inPosition;
layout(location = 1) in vec4 inColor;

// Output to fragment shader
layout(location = 0) out vec4 fragColor;

// Uniforms via push constants
layout(push_constant) uniform Uniforms {
    mat4 model;
} uniforms;

void main() {
    // Apply rotation transform
    vec4 pos = uniforms.model * vec4(inPosition, 0.0, 1.0);
    pos.z = 0.5; // Set z to middle of depth range
    gl_Position = pos;

    // Pass color to fragment shader
    fragColor = inColor;
}