export const borderVertexShader = `#version 300 es
precision mediump float;

in vec2 a_position;
uniform vec2 u_resolution;
uniform vec4 u_rect;

out vec2 v_position;

void main() {
  vec2 position = a_position * u_rect.zw + u_rect.xy;
  v_position = a_position * u_rect.zw;

  vec2 clipSpace = (position / u_resolution) * 2.0 - 1.0;
  gl_Position = vec4(clipSpace * vec2(1, -1), 0, 1);
}
`;

export const borderFragmentShader = `#version 300 es
precision mediump float;

uniform mediump vec4 u_color;
uniform mediump vec4 u_rect;         // x, y, width, height
uniform mediump vec4 u_cornerRadius; // topLeft, topRight, bottomLeft, bottomRight
uniform mediump vec4 u_borderWidth;  // left, right, top, bottom

in vec2 v_position;

out vec4 fragColor;

// SDF for rounded rectangle (reused from rectangle shader)
float roundedBoxSDF(vec2 p, vec2 size, vec4 radius) {
  float r = (p.x < 0.0) ?
            ((p.y < 0.0) ? radius.x : radius.z) :
            ((p.y < 0.0) ? radius.y : radius.w);

  vec2 q = abs(p) - size + r;
  return min(max(q.x, q.y), 0.0) + length(max(q, 0.0)) - r;
}

void main() {
  vec2 halfSize = u_rect.zw * 0.5;
  vec2 center = v_position - halfSize;

  // Outer rounded box
  float outerDist = roundedBoxSDF(center, halfSize, u_cornerRadius);

  // Inner box: shrink by border width
  vec2 innerFullSize = u_rect.zw - vec2(
    u_borderWidth.x + u_borderWidth.y,  // left + right
    u_borderWidth.z + u_borderWidth.w   // top + bottom
  );
  vec2 innerHalfSize = innerFullSize * 0.5;

  // Inner corner radius
  vec4 innerRadius = max(
    u_cornerRadius - vec4(
      max(u_borderWidth.x, u_borderWidth.z),
      max(u_borderWidth.y, u_borderWidth.z),
      max(u_borderWidth.x, u_borderWidth.w),
      max(u_borderWidth.y, u_borderWidth.w)
    ),
    vec4(0.0)
  );

  // If border consumes everything, fill it
  if (innerHalfSize.x <= 0.0 || innerHalfSize.y <= 0.0) {
    float alpha = 1.0 - smoothstep(-0.5, 0.5, outerDist);
    fragColor = vec4(u_color.rgb, u_color.a * alpha);
    return;
  }

  // Inner box SDF
  float innerDist = roundedBoxSDF(center, innerHalfSize, innerRadius);

  // Discard pixels inside the inner box (let the background rectangle show through)
  if (innerDist < 0.0) {
    discard;
  }

  // Border region (inside outer, outside inner)
  if (outerDist < 0.0) {
    float edgeWidth = length(vec2(dFdx(outerDist), dFdy(outerDist)));
    edgeWidth = max(edgeWidth, 0.5);
    float alpha = 1.0 - smoothstep(-edgeWidth, edgeWidth, outerDist);
    fragColor = vec4(u_color.rgb, u_color.a * alpha);
    return;
  }

  // Outside everything
  discard;
}
`;
