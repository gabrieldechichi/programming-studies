@vs vs
layout(location=0) in vec2 position;
layout(location=1) in vec4 color;

layout(binding=0) uniform vs_params {
    mat4 model;
};

out vec4 vs_color;

void main() {
    gl_Position = model * vec4(position, 0.0, 1.0);
    vs_color = color;
}
@end

@fs fs
in vec4 vs_color;
out vec4 frag_color;

void main() {
    frag_color = vs_color;
}
@end

@program triangle vs fs