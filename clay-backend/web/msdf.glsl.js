// MSDF Text Rendering Shaders

export const msdfVertexShader = `#version 300 es
precision highp float;

in vec2 a_position;

uniform vec2 u_resolution;
uniform vec4 u_rect; // x, y, width, height (screen space)
uniform vec4 u_uvRect; // u0, v0, u1, v1 (atlas UVs)

out vec2 v_texCoord;

void main() {
  // Calculate screen position
  vec2 position = u_rect.xy + (a_position * u_rect.zw);

  // Calculate UV coordinates
  v_texCoord = u_uvRect.xy + (a_position * u_uvRect.zw);

  // Convert to clip space
  vec2 clipSpace = (position / u_resolution) * 2.0 - 1.0;
  clipSpace.y *= -1.0;  // Flip Y for top-left origin

  gl_Position = vec4(clipSpace, 0.0, 1.0);
}
`;

export const msdfFragmentShader = `#version 300 es
precision highp float;

in vec2 v_texCoord;
out vec4 fragColor;

uniform sampler2D u_atlas;
uniform vec4 u_color;

// MSDF median function
float median(vec3 rgb) {
  return max(min(rgb.r, rgb.g), min(max(rgb.r, rgb.g), rgb.b));
}

void main() {
  // Sample MSDF texture
  vec3 msdf = texture(u_atlas, v_texCoord).rgb;

  // Calculate signed distance
  float dist = median(msdf);

  // MSDF range in pixels (typical value for msdf-atlas-gen with padding=2)
  float pxRange = 4.0;

  // Convert to screen-space distance
  vec2 unitRange = vec2(pxRange) / vec2(textureSize(u_atlas, 0));
  vec2 screenTexSize = vec2(1.0) / fwidth(v_texCoord);
  float screenPxRange = max(0.5 * dot(unitRange, screenTexSize), 1.0);
  float screenPxDistance = screenPxRange * (dist - 0.5);

  // Smooth alpha with antialiasing
  float alpha = clamp(screenPxDistance + 0.5, 0.0, 1.0);

  // Output with text color
  fragColor = vec4(u_color.rgb, u_color.a * alpha);
}
`;
