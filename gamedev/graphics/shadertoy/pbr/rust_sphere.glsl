#include "./lib/consts.glsl"
#include "./lib/math.glsl"
const float MAX_RM_DISTANCE = 200.0;
const float MIN_RM_DISTANCE = EPSILON;
const int MAX_RM_IT = 100;
float sdSphere(vec3 p, float radius) {
    return length(p) - radius;
}

struct RaymarchResult {
    float distance;
    vec3 color;
};

// Simplex Noise Functions
vec4 permute(vec4 x) {
    return mod(((x * 34.0) + 1.0) * x, 289.0);
}
vec4 taylorInvSqrt(vec4 r) {
    return 1.79284291400159 - 0.85373472095314 * r;
}
vec3 fade(vec3 t) {
    return t * t * t * (t * (t * 6.0 - 15.0) + 10.0);
}

float simplexNoise(vec3 v)
{
    const vec2 C = vec2(1.0 / 6.0, 1.0 / 3.0);
    const vec4 D = vec4(0.0, 0.5, 1.0, 2.0);

    vec3 i = floor(v + dot(v, C.yyy));
    vec3 x0 = v - i + dot(i, C.xxx);

    vec3 g = step(x0.yzx, x0.xyz);
    vec3 l = 1.0 - g;
    vec3 i1 = min(g.xyz, l.zxy);
    vec3 i2 = max(g.xyz, l.zxy);

    vec3 x1 = x0 - i1 + 1.0 * C.xxx;
    vec3 x2 = x0 - i2 + 2.0 * C.xxx;
    vec3 x3 = x0 - 1. + 3.0 * C.xxx;

    i = mod(i, 289.0);
    vec4 p = permute(permute(permute(
                    i.z + vec4(0.0, i1.z, i2.z, 1.0))
                    + i.y + vec4(0.0, i1.y, i2.y, 1.0))
                + i.x + vec4(0.0, i1.x, i2.x, 1.0));

    float n_ = 1.0 / 7.0;
    vec3 ns = n_ * D.wyz - D.xzx;

    vec4 j = p - 49.0 * floor(p * ns.z * ns.z);

    vec4 x_ = floor(j * ns.z);
    vec4 y_ = floor(j - 7.0 * x_);

    vec4 x = x_ * ns.x + ns.yyyy;
    vec4 y = y_ * ns.x + ns.yyyy;
    vec4 h = 1.0 - abs(x) - abs(y);

    vec4 b0 = vec4(x.xy, y.xy);
    vec4 b1 = vec4(x.zw, y.zw);

    vec4 s0 = floor(b0) * 2.0 + 1.0;
    vec4 s1 = floor(b1) * 2.0 + 1.0;
    vec4 sh = -step(h, vec4(0.0));

    vec4 a0 = b0.xzyw + s0.xzyw * sh.xxyy;
    vec4 a1 = b1.xzyw + s1.xzyw * sh.zzww;

    vec3 p0 = vec3(a0.xy, h.x);
    vec3 p1 = vec3(a0.zw, h.y);
    vec3 p2 = vec3(a1.xy, h.z);
    vec3 p3 = vec3(a1.zw, h.w);

    vec4 norm = taylorInvSqrt(vec4(dot(p0, p0), dot(p1, p1), dot(p2, p2), dot(p3, p3)));
    p0 *= norm.x;
    p1 *= norm.y;
    p2 *= norm.z;
    p3 *= norm.w;

    vec4 m = max(0.6 - vec4(dot(x0, x0), dot(x1, x1), dot(x2, x2), dot(x3, x3)), 0.0);
    m = m * m;
    return 42.0 * dot(m * m, vec4(dot(p0, x0), dot(p1, x1), dot(p2, x2), dot(p3, x3)));
}

RaymarchResult map(vec3 p) {
    p = rotateY(p, iTime);
    RaymarchResult r;
    r.distance = sdSphere(p, 0.5);

    // Layered noise for rust effect
    float noise1 = simplexNoise(p * 3.0);
    float noise2 = simplexNoise(p * 10.0);

    // Adding higher-frequency noise for edge irregularity
    float highFreqNoise = simplexNoise(p * 50.0) * 0.3; // Adjust scaling factor as needed

    float rustFactor = smoothstep(0.0, 0.5, mix(noise1, noise2, 0.5) + highFreqNoise);

    vec3 metalColor = vec3(0.8, 0.8, 0.8);
    vec3 rustColor = metalColor * 0.3;
    rustColor.r += 0.1;
    r.color = mix(metalColor, rustColor, rustFactor);

    return r;
}

vec3 normals(in vec3 pos) {
    vec2 e = vec2(1.0, -1.0) * 0.5773;
    const float eps = EPSILON;
    return normalize(e.xyy * map(pos + e.xyy * eps).distance +
            e.yyx * map(pos + e.yyx * eps).distance +
            e.yxy * map(pos + e.yxy * eps).distance +
            e.xxx * map(pos + e.xxx * eps).distance);
}

// Surface Distance Function

void mainImage(out vec4 fragColor, in vec2 fragCoord) {
    vec2 uv = (2.0 * fragCoord - iResolution.xy) / iResolution.y;

    vec3 ro = vec3(0., 0., -3.5);
    vec3 rd = normalize(vec3(uv.x, uv.y, 1.));

    vec3 lightDir = normalize(vec3(1.0, 1.0, -1.0));
    vec3 ambientLight = vec3(0.3, 0.3, 0.3);

    vec3 col = vec3(0.1, 0.7, 0.7);
    float t = 0.;

    RaymarchResult r;
    for (int i = 0; i < MAX_RM_IT; i++) {
        vec3 p = ro + rd * t;
        r = map(p);
        t += r.distance;
        if (r.distance < MIN_RM_DISTANCE) break;
        if (t > MAX_RM_DISTANCE) break;
    }

    if (t < MAX_RM_DISTANCE) {
        vec3 pos = ro + rd * t;
        vec3 normal = normals(pos);

        // Phong reflection model
        vec3 viewDir = normalize(ro - pos);
        vec3 halfDir = normalize(lightDir + viewDir);
        float specularStrength = 0.5;

        float shininess = 32.0; // Lower value for rougher surface
        float specFactor = pow(max(dot(normal, halfDir), 0.0), shininess);
        vec3 specular = specularStrength * specFactor * vec3(1.0); // White specular highlight

        float diffuse = max(dot(normal, lightDir), 0.0);
        float ambFactor = 0.5 + 0.5 * dot(normal, -vec3(1.0, 1.0, 0.0));
        vec3 diffuseColor = vec3(diffuse) * r.color;

        col = diffuseColor + ambientLight * ambFactor + specular;
    }

    fragColor = vec4(col, 1.0);
}
