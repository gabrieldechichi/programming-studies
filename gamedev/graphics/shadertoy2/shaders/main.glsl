#include "./lib/consts.glsl"
#include "./lib/math.glsl"
#include "./lib/raymarch.glsl"

void mainImage(out vec4 fragColor, in vec2 fragCoord) {
    vec2 uv = (2.0 * fragCoord - iResolution.xy) / iResolution.y;
    fragColor = vec4(uv, 0., 1.);
}
