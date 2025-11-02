export const imageVertexShader = `#version 300 es
precision mediump float;

in vec2 a_position;
uniform vec2 u_resolution;
uniform vec4 u_rect;

out vec2 v_texCoord;
out vec2 v_position;  // For corner radius SDF

void main() {
  vec2 position = a_position * u_rect.zw + u_rect.xy;
  v_texCoord = a_position;
  v_position = a_position * u_rect.zw;  // Local space

  vec2 clipSpace = (position / u_resolution) * 2.0 - 1.0;
  gl_Position = vec4(clipSpace * vec2(1, -1), 0, 1);
}
`;

export const imageFragmentShader = `#version 300 es
precision mediump float;

uniform sampler2D u_texture;
uniform vec4 u_tint;
uniform vec4 u_rect;
uniform vec4 u_cornerRadius;

in vec2 v_texCoord;
in vec2 v_position;
out vec4 fragColor;

// Same SDF as rectangle shader
float roundedBoxSDF(vec2 p, vec2 size, vec4 radius) {
  float r = (p.x < 0.0) ?
            ((p.y < 0.0) ? radius.x : radius.z) :
            ((p.y < 0.0) ? radius.y : radius.w);

  vec2 q = abs(p) - size + r;
  return min(max(q.x, q.y), 0.0) + length(max(q, 0.0)) - r;
}

void main() {
  // Sample texture
  vec4 texColor = texture(u_texture, v_texCoord);

  // Apply tint
  vec4 finalColor = texColor * u_tint;

  // Apply corner radius masking
  vec2 halfSize = u_rect.zw * 0.5;
  vec2 center = v_position - halfSize;
  float dist = roundedBoxSDF(center, halfSize, u_cornerRadius);

  // Antialiasing
  float edgeWidth = length(vec2(dFdx(dist), dFdy(dist)));
  edgeWidth = max(edgeWidth, 0.5);
  float alpha = 1.0 - smoothstep(-edgeWidth, edgeWidth, dist);

  fragColor = vec4(finalColor.rgb, finalColor.a * alpha);
}
`;
