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

float sdCylinder(vec3 p, vec3 a, vec3 b, float r)
{
    vec3 ba = b - a;
    vec3 pa = p - a;
    float baba = dot(ba, ba);
    float paba = dot(pa, ba);
    float x = length(pa * baba - ba * paba) - r * baba;
    float y = abs(paba - baba * 0.5) - baba * 0.5;
    float x2 = x * x;
    float y2 = y * y * baba;

    float d = (max(x, y) < 0.0) ? -min(x2, y2) : (((x > 0.0) ? x2 : 0.0) + ((y > 0.0) ? y2 : 0.0));

    return sign(d) * sqrt(abs(d)) / baba;
}

float sdCylinderY(vec3 p, float h, float r)
{
    vec2 d = abs(vec2(length(p.xz), p.y)) - vec2(r, h);
    return min(max(d.x, d.y), 0.0) + length(max(d, 0.0));
}
float sdCylinderX(vec3 p, float h, float r)
{
    vec2 d = abs(vec2(length(p.yz), p.x)) - vec2(r, h);
    return min(max(d.x, d.y), 0.0) + length(max(d, 0.0));
}

float sdCylinderZ(vec3 p, float h, float r)
{
    vec2 d = abs(vec2(length(p.xy), p.z)) - vec2(r, h);
    return min(max(d.x, d.y), 0.0) + length(max(d, 0.0));
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

vec3 rotateYY(vec3 point, float angle)
{
    float r = angle;
    float x = point.x * sin(r) - point.z * cos(r);
    float z = point.x * cos(r) + point.z * sin(r);
    return vec3(x, point.y, z);
}

float map(vec3 p) {
    p = rotateYY(p, iTime);
    return sdCylinderZ(p, 0.15, 1.0);
}

vec3 calcNormal(in vec3 pos)
{
    vec2 e = vec2(1.0, -1.0) * 0.5773;
    const float eps = EPSILON;
    return normalize(e.xyy * map(pos + e.xyy * eps) +
            e.yyx * map(pos + e.yyx * eps) +
            e.yxy * map(pos + e.yxy * eps) +
            e.xxx * map(pos + e.xxx * eps));
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
    vec3 lightDir = vec3(0.5773);
    vec3 lightCol = vec3(1.0);

    float t = 0.;
    for (int i = 0; i < MAX_RM_IT; i++) {
        vec3 p = ro + rd * t;
        float d = map(p);
        t += d;
        if (d < MIN_RM_DISTANCE) break;
        if (t > MAX_RM_DISTANCE) break;
    }

    if (t < MAX_RM_DISTANCE) {
        vec3 pos = ro + rd * t;
        vec3 normal = calcNormal(pos);
        float diffuse = max(dot(normal, lightDir), 0.0);
        vec3 amb = vec3(0.2);
        col = vec3(diffuse) + amb;
        col = normal;
    }
    // vec3 bg = background(uv);
    // col = mix(col, bg, step(MAX_RM_DISTANCE, t));

    fragColor = vec4(col, 1.0);
}
