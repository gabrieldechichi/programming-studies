// Text rendering shaders

export const textVertexShader = `#version 300 es
in vec2 a_position;

out vec2 v_texCoord;

uniform vec2 u_resolution;
uniform vec4 u_rect; // x, y, width, height

void main() {
    // Calculate texture coordinates (0,0 to 1,1)
    v_texCoord = a_position;

    // Transform quad to glyph position
    vec2 position = u_rect.xy + (a_position * u_rect.zw);

    // Convert to clip space (-1 to 1)
    vec2 clipSpace = (position / u_resolution) * 2.0 - 1.0;
    clipSpace.y *= -1.0;  // Flip Y (canvas is top-left origin)

    gl_Position = vec4(clipSpace, 0.0, 1.0);
}
`;

export const textFragmentShader = `#version 300 es
precision highp float;

in vec2 v_texCoord;
out vec4 fragColor;

uniform sampler2D u_texture;
uniform vec4 u_color;

void main() {
    // Sample alpha channel from texture (single-channel stored in red)
    float alpha = texture(u_texture, v_texCoord).r;

    // Output color with sampled alpha for anti-aliasing
    fragColor = vec4(u_color.rgb, u_color.a * alpha);
}
`;
