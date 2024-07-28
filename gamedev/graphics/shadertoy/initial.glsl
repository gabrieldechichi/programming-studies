#include "./lib/consts.glsl"
#include "./lib/math.glsl"

float opSmoothUnion(float d1, float d2, float k)
{
    float h = clamp(0.5 + 0.5 * (d2 - d1) / k, 0.0, 1.0);
    return mix(d2, d1, h) - k * h * (1.0 - h);
}

float opSmoothSubtraction(float d1, float d2, float k)
{
    float h = clamp(0.5 - 0.5 * (d2 + d1) / k, 0.0, 1.0);
    return mix(d2, -d1, h) + k * h * (1.0 - h);
}

float opSmoothIntersection(float d1, float d2, float k)
{
    float h = clamp(0.5 - 0.5 * (d2 - d1) / k, 0.0, 1.0);
    return mix(d2, d1, h) + k * h * (1.0 - h);
}

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

float sdSphere(vec3 p, float radius) {
    return length(p) - radius;
}

float sdBox(vec3 p, vec3 bounds) {
    return length(max(abs(p) - bounds, 0.0));
}

float rayMarchScene(vec3 p) {
    vec4 sphere = vec4(1.5 * sin(iTime), 0., 0.0, 1.);
    float sphereDist = sdSphere(p - sphere.xyz, sphere.w);

    float boxDist = sdBox(p, vec3(0.75));
    return opSmoothUnion(sphereDist, boxDist, 0.5);
}

const float MAX_RM_DISTANCE = 200.0;
const float MIN_RM_DISTANCE = EPSILON;
const int MAX_RM_IT = 2500;

void mainImage(out vec4 fragColor, in vec2 fragCoord)
{
    vec2 uv = (2.0 * fragCoord - iResolution.xy) / iResolution.y;

    vec3 ro = vec3(0., 0., -3.);
    vec3 rd = normalize(vec3(uv, 1.));
    vec3 col = vec3(0.);

    float t = 0.;
    for (int i = 0; i < MAX_RM_IT; i++) {
        vec3 p = ro + rd * t;
        float d = rayMarchScene(p);
        t += d;
        if (d < MIN_RM_DISTANCE) break;
        if (t > MAX_RM_DISTANCE) break;
    }

    col = vec3(t * 0.2);
    vec3 bg = background(uv);
    col = mix(col, bg, step(MAX_RM_DISTANCE, t));

    fragColor = vec4(col, 1.0);
}
