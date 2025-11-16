export const rectangleVertexShader = `#version 300 es
precision mediump float;

in vec2 a_position;
uniform vec2 u_resolution;
uniform vec4 u_rect;

out vec2 v_position;  // Position in rect local space

void main() {
  vec2 position = a_position * u_rect.zw + u_rect.xy;
  v_position = a_position * u_rect.zw;  // Local position for fragment shader

  vec2 clipSpace = (position / u_resolution) * 2.0 - 1.0;
  gl_Position = vec4(clipSpace * vec2(1, -1), 0, 1);
}
`;

export const rectangleFragmentShader = `#version 300 es
precision mediump float;

uniform mediump vec4 u_color;
uniform mediump vec4 u_rect;         // x, y, width, height
uniform mediump vec4 u_cornerRadius; // topLeft, topRight, bottomLeft, bottomRight

in vec2 v_position;  // Pixel position in rect space (0-width, 0-height)

out vec4 fragColor;

// SDF for rounded rectangle
float roundedBoxSDF(vec2 p, vec2 size, vec4 radius) {
  // Select radius based on quadrant
  float r = (p.x < 0.0) ?
            ((p.y < 0.0) ? radius.x : radius.z) :  // topLeft : bottomLeft
            ((p.y < 0.0) ? radius.y : radius.w);   // topRight : bottomRight

  vec2 q = abs(p) - size + r;
  return min(max(q.x, q.y), 0.0) + length(max(q, 0.0)) - r;
}

void main() {
  // Convert to centered coordinates
  vec2 halfSize = u_rect.zw * 0.5;
  vec2 center = v_position - halfSize;

  // Calculate SDF distance
  float dist = roundedBoxSDF(center, halfSize, u_cornerRadius);

  // High-quality antialiasing using screen-space derivatives
  float edgeWidth = length(vec2(dFdx(dist), dFdy(dist)));

  // Clamp to avoid artifacts on very small shapes
  edgeWidth = max(edgeWidth, 0.5);

  // Smooth transition
  float alpha = 1.0 - smoothstep(-edgeWidth, edgeWidth, dist);

  fragColor = vec4(u_color.rgb, u_color.a * alpha);
}
`;
