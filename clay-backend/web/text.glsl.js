// Text rendering shaders

export const textVertexShader = `#version 300 es
in vec2 a_position;

out vec2 v_texCoord;

uniform vec2 u_resolution;
uniform vec4 u_rect; // x, y, width, height
uniform vec4 u_uvBounds; // u0, v0, u1, v1

void main() {
    // Remap texture coordinates using UV bounds
    v_texCoord = mix(u_uvBounds.xy, u_uvBounds.zw, a_position);

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
uniform float u_distanceRange;
uniform float u_fontSize;

float median(float r, float g, float b) {
    return max(min(r, g), min(max(r, g), b));
}

void main() {
    // Sample MSDF texture (RGB channels contain distance field)
    vec3 msdf = texture(u_texture, v_texCoord).rgb;

    // Get median distance
    float dist = median(msdf.r, msdf.g, msdf.b);

    // Use fwidth for automatic antialiasing based on screen-space derivatives
    float width = fwidth(dist);
    float alpha = smoothstep(0.5 - width, 0.5 + width, dist);

    // Output color with calculated alpha
    fragColor = vec4(u_color.rgb, u_color.a * alpha);
}
`;
