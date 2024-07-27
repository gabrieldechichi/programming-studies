#include "./lib/consts.glsl"
#include "./lib/math.glsl"

float vignette(vec2 uv, float r) {
    return min(1.0 - length(uv) + r, 1.0);
}

vec3 background(vec2 uv) {
    uv = rotate2d(uv, PI * 0.25);

    vec2 uvx = mod(uv * 150.0, vec2(4.0, 8.0)) - vec2(2.0, 2.0);
    vec2 uvy = mod(uv * 150.0 + vec2(2.0, 4.0), vec2(4.0, 8.0)) - vec2(2.0, 2.0);
    float cx = rectangle(uvx, vec2(1.0, 1.5));
    float cy = rectangle(uvy, vec2(1.5, 1.0));

    float factor = cx + cy;

    vec3 grid = mix(vec3(0.370, 0.004, 0.011), vec3(0.520, 0.100, 0.018), cx + cy);

    return grid * vignette(uv, 0.4);
}

void mainImage(out vec4 fragColor, in vec2 fragCoord)
{
    vec2 uv = (2.0 * fragCoord - iResolution.xy) / iResolution.y;

    vec3 col = background(uv);

    fragColor = vec4(col, 1.0);
}
