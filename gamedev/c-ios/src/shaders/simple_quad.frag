#version 450

// Inputs from vertex shader
layout(location = 0) in vec2 vTexCoord;

// Texture binding
layout(binding = 2) uniform sampler2D uTexture;

// Output
layout(location = 0) out vec4 FragColor;

void main() {
    // Debug: Output solid red color
    // FragColor = vec4(0.0, 0.0, 1.0, 1.0);

    // Original texture sampling (commented for debugging)
    vec4 textureColor = texture(uTexture, vTexCoord);
    FragColor = textureColor;
}