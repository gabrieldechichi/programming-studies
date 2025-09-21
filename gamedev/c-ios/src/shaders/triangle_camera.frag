#version 450

// Input from vertex shader
layout(location = 0) in vec4 fragColor;
layout(location = 1) in vec3 vs_world_pos;
layout(location = 2) in vec3 vs_view_pos;

// Output
layout(location = 0) out vec4 outColor;

void main() {
    // Simple pass-through of vertex color
    // You could add lighting or other effects here using world_pos and view_pos
    outColor = fragColor;
}