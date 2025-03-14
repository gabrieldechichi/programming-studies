/* "Deadpool + Wolverine - https://www.shadertoy.com/view/msj3D3

   This work is licensed under a Creative Commons Attribution-NonCommercial-ShareAlike 4.0 International License (https://creativecommons.org/licenses/by-nc-sa/4.0/deed.en)

   Got inspired by the new move and decided to make this using raymarching.
   Ended up being a fun challenge, and I had to use several sd functions and operations.
   Getting Wolverine "eye frame" was particularly challenging.

   Thanks to Inigo Quilez for the distance functions and the amazing educational content.
   https://iquilezles.org/articles/distfunctions/

   Inspiration:
   - https://www.shadertoy.com/view/4dcfz7
   - https://www.pinterest.com/pin/deadpool-x-wolverine-logo-png-in-2024--1006413847967142752/
*/

#include "./lib/consts.glsl"
#include "./lib/math.glsl"
const float MAX_RM_DISTANCE = 200.0;
const float MIN_RM_DISTANCE = EPSILON;
const int MAX_RM_IT = 100;
#define ROTATE 1
#define ROTATE_SPEED iTime

float opUnion(float d1, float d2)
{
    return min(d1, d2);
}
// d1 - d2
float opSubtraction(float d1, float d2)
{
    return max(-d2, d1);
}

float opSmoothUnion(float d1, float d2, float k)
{
    float h = clamp(0.5 + 0.5 * (d2 - d1) / k, 0.0, 1.0);
    return mix(d2, d1, h) - k * h * (1.0 - h);
}

float sdTriPrismZ(vec3 p, vec2 h)
{
    vec3 q = abs(p);
    return max(q.z - h.y, max(q.x * 0.866025 + p.y * 0.5, -p.y) - h.x * 0.5);
}

// la,lb=semi axis, h=height, ra=corner
float sdRhombus(vec3 p, float la, float lb, float h, float ra)
{
    p = abs(p);
    vec2 b = vec2(la, lb);
    float f = clamp((ndot(b, b - 2.0 * p.yx)) / dot(b, b), -1.0, 1.0);
    vec2 q = vec2(
            length(p.yx - 0.5 * b * vec2(1.0 - f, 1.0 + f))
                * sign(p.y * b.y + p.x * b.x - b.x * b.y)
                - ra, p.z - h);
    return min(max(q.x, q.y), 0.0) + length(max(q, 0.0));
}

float sdCylinderZ(vec3 p, float h, float r)
{
    vec2 d = abs(vec2(length(p.xy), p.z)) - vec2(r, h);
    return min(max(d.x, d.y), 0.0) + length(max(d, 0.0));
}

float vignette(vec2 uv, float r) {
    return min(1.0 - length(uv) + r, 1.0);
}

vec3 background(vec2 uv, vec3 col1, vec3 col2) {
    uv = rotate2d(uv, PI * 0.25);

    vec2 uvx = mod(uv * 150.0, vec2(4.0, 8.0)) - vec2(2.0, 2.0);
    vec2 uvy = mod(uv * 150.0 + vec2(2.0, 4.0), vec2(4.0, 8.0)) - vec2(2.0, 2.0);
    float cx = rectangle(uvx, vec2(1.0, 1.5));
    float cy = rectangle(uvy, vec2(1.5, 1.0));

    float factor = cx + cy;

    vec3 grid = mix(col1, col2, cx + cy);

    return grid * vignette(uv, 0.4);
}

float sdSphere(vec3 p, float radius) {
    return length(p) - radius;
}

float sdBox(vec3 p, vec3 b)
{
    vec3 q = abs(p) - b;
    return length(max(q, 0.0)) + min(max(q.x, max(q.y, q.z)), 0.0);
}

struct RaymarchResult {
    float distance;
    vec3 color;
};

RaymarchResult eyeDeadpool(vec3 p, float mult) {
    RaymarchResult r;

    p.xy -= vec2(0.55 * mult, 0.1);

    float cylinder = sdCylinderZ(vec3(p.x / 1.2, p.y / 1.1, p.z), 0.15, 0.2);
    float cylinder2 = sdCylinderZ(vec3((p.x / 1.2) - 0.22, (p.y / 1.1) - 0.2, p.z), 0.72, 0.3);

    r.distance = opSubtraction(cylinder, cylinder2);
    r.color = vec3(1.0);
    return r;
}

RaymarchResult eyeWolverine(vec3 p, float mult) {
    RaymarchResult r;
    p.xy -= vec2(0.58 * mult, 0.1);
    p.xy = rotate2d(p.xy, PI * 0.75 * mult);
    r.distance = sdTriPrismZ(vec3(p.x / 1.7, p.y * 1.2, p.z), vec2(0.21, 0.17));
    r.color = vec3(1.0);
    return r;
}

float heart(vec3 p, float scale, float z) {
    p /= scale;
    float cylinderLeft = sdCylinderZ(p - vec3(0.5, 0.15, 0.), z, .6);
    float cylinderRight = sdCylinderZ(p - vec3(-0.5, 0.15, 0.), z, .6);
    p.xy = rotate2d(p.xy, PI);
    p -= vec3(0., 0.55, 0.);
    p.x /= 2.26;
    p.y /= 1.4;
    float triangle = sdTriPrismZ(p, vec2(0.51, z));
    return opUnion(
        triangle,
        opUnion(cylinderLeft, cylinderRight));
}

RaymarchResult logoBase(vec3 p, vec3 innerCol, vec3 outerCol) {
    RaymarchResult r;
    float internalHeart = heart(p, 1.1, 0.1);

    float externalHeart = opSubtraction(heart(p, 1.2, 0.127), heart(p, 0.95, 0.888));
    float outerBox = sdBox(p + vec3(0., 0.3, 0.), vec3(0.12, 0.9, 0.15));
    externalHeart = opUnion(externalHeart, outerBox);

    if (internalHeart < externalHeart) {
        r.distance = internalHeart;
        r.color = innerCol;
    } else {
        r.distance = externalHeart;
        r.color = outerCol;
    }

    return r;

    // RaymarchResult r;
    // float innerCylinder = sdCylinderZ(p, 0.10, 1.0);
    // r.color = innerCol;
    // r.distance = innerCylinder;
    //
    // float outerCylinder1 = sdCylinderZ(p, 0.15, 1.2);
    // float outerCylinder2 = sdCylinderZ(p, 0.4, 1.0);
    // float outerBox = sdBox(p, vec3(0.15, 1.20 - 0.01, 0.15));
    // float outerCylinder = opSubtraction(outerCylinder1, outerCylinder2);
    // outerCylinder = opUnion(outerBox, outerCylinder);
    // if (outerCylinder < r.distance) {
    //     r.color = outerCol;
    //     r.distance = outerCylinder;
    // }
    //
    // return r;
}

// RaymarchResult logoBase(vec3 p, vec3 innerCol, vec3 outerCol) {
//     RaymarchResult r;
//     float innerCylinder = sdCylinderZ(p, 0.10, 1.0);
//     r.color = innerCol;
//     r.distance = innerCylinder;
//
//     float outerCylinder1 = sdCylinderZ(p, 0.15, 1.2);
//     float outerCylinder2 = sdCylinderZ(p, 0.4, 1.0);
//     float outerBox = sdBox(p, vec3(0.15, 1.20 - 0.01, 0.15));
//     float outerCylinder = opSubtraction(outerCylinder1, outerCylinder2);
//     outerCylinder = opUnion(outerBox, outerCylinder);
//     if (outerCylinder < r.distance) {
//         r.color = outerCol;
//         r.distance = outerCylinder;
//     }
//
//     return r;
// }

//BEGIN DEADPOOL
RaymarchResult raymarchDeapool(vec3 p) {
    #if ROTATE==1
    p = rotateY(p, ROTATE_SPEED);
    #endif
    RaymarchResult r = logoBase(p, vec3(0.12), vec3(0.7, 0.1, 0.1));

    RaymarchResult eyeRight = eyeDeadpool(p, 1.);
    RaymarchResult eyeLeft = eyeDeadpool(p, -1.);

    if (eyeRight.distance < r.distance) {
        r = eyeRight;
    }
    if (eyeLeft.distance < r.distance) {
        r = eyeLeft;
    }

    float minusBox = sdBox(p - vec3(10.0, 0.0, 0.0), vec3(10., 10., 3.));
    r.distance = opSubtraction(r.distance, minusBox);
    return r;
}

vec3 normalsDeadpool(in vec3 pos)
{
    vec2 e = vec2(1.0, -1.0) * 0.5773;
    const float eps = EPSILON;
    return normalize(e.xyy * raymarchDeapool(pos + e.xyy * eps).distance +
            e.yyx * raymarchDeapool(pos + e.yyx * eps).distance +
            e.yxy * raymarchDeapool(pos + e.yxy * eps).distance +
            e.xxx * raymarchDeapool(pos + e.xxx * eps).distance);
}

vec3 calculateLighting(vec3 color, vec3 normal, vec3 lightDir, vec3 viewDir, vec3 ambientLight) {
    float diffuse = max(dot(normal, -lightDir), 0.0);
    float ambFactor = 0.5 + 0.5 * dot(normal, vec3(0.0, 1.0, 0.0));
    vec3 diffuseColor = vec3(diffuse) * color;

    vec3 halfDir = normalize(-lightDir + viewDir);
    float specularStrength = 1.;
    float shininess = 82.0;
    float specFactor = pow(max(dot(normal, halfDir), 0.0), shininess);
    vec3 specular = specularStrength * specFactor * vec3(1.0); // White specular highlight

    return diffuseColor + ambientLight * ambFactor + specular;
}

void logoDeadPool(in vec3 ro, in vec3 rd,
    in vec3 lightDir, in vec3 ambientLight,
    out vec3 color, out float t)
{
    RaymarchResult r;
    for (int i = 0; i < MAX_RM_IT; i++) {
        vec3 p = ro + rd * t;
        r = raymarchDeapool(p);
        t += r.distance;
        if (r.distance < MIN_RM_DISTANCE) break;
        if (t > MAX_RM_DISTANCE) break;
    }

    if (t < MAX_RM_DISTANCE) {
        vec3 pos = ro + rd * t;
        vec3 normal = normalsDeadpool(pos);
        vec3 viewDir = normalize(ro - pos);
        color = calculateLighting(r.color, normal, lightDir, viewDir, ambientLight);
    }
}
//END DEADPOOL

//BEGIN WOLVERINE
RaymarchResult eyeFrame(vec3 p, float sign) {
    vec2 baseOffset = vec2(0.9 * sign, 0.45);
    vec3 p1 = p;
    p1.xy -= baseOffset;
    p1.xy = rotate2d(p1.xy, -PI * 1.75 * sign);
    float tri1 = sdRhombus(p1, 0.99, 0.35, 0.12, 0.06);

    vec3 p2 = p;
    p2.xy -= baseOffset + vec2(0.09 * sign, 0.1);
    p2.xy = rotate2d(p2.xy, -PI * 1.78 * sign);
    float tri2 = sdRhombus(p2, 1.0, 0.4, 0.12, 0.06);

    tri1 = opSmoothUnion(tri1, tri2, 0.15);
    RaymarchResult r;
    r.distance = tri1;
    r.color = vec3(0.1);
    return r;
}

RaymarchResult raymarchWolverine(vec3 p) {
    #if ROTATE==1
    p = rotateY(p, ROTATE_SPEED);
    #endif
    RaymarchResult r = logoBase(p, vec3(.7, .7, 0.0), vec3(0.9, 0.9, 0.1));

    RaymarchResult eyeRight = eyeWolverine(p, 1.);
    RaymarchResult eyeLeft = eyeWolverine(p, -1.);

    if (eyeRight.distance < r.distance) {
        r = eyeRight;
    }
    if (eyeLeft.distance < r.distance) {
        r = eyeLeft;
    }

    RaymarchResult eyeFrameRight = eyeFrame(p, 1.);
    RaymarchResult eyeFrameLeft = eyeFrame(p, -1.);

    if (eyeFrameRight.distance < r.distance) {
        r = eyeFrameRight;
    }
    if (eyeFrameLeft.distance < r.distance) {
        r = eyeFrameLeft;
    }

    float minusBox = sdBox(p + vec3(10.0, 0.0, 0.0), vec3(10., 10., 3.));
    r.distance = opSubtraction(r.distance, minusBox);

    return r;
}

vec3 normalsWolverine(in vec3 pos)
{
    vec2 e = vec2(1.0, -1.0) * 0.5773;
    const float eps = EPSILON;
    return normalize(e.xyy * raymarchWolverine(pos + e.xyy * eps).distance +
            e.yyx * raymarchWolverine(pos + e.yyx * eps).distance +
            e.yxy * raymarchWolverine(pos + e.yxy * eps).distance +
            e.xxx * raymarchWolverine(pos + e.xxx * eps).distance);
}

void logoWolverine(in vec3 ro, in vec3 rd,
    in vec3 lightDir, in vec3 ambientLight,
    out vec3 color, out float t)
{
    RaymarchResult r;
    for (int i = 0; i < MAX_RM_IT; i++) {
        vec3 p = ro + rd * t;
        r = raymarchWolverine(p);
        t += r.distance;
        if (r.distance < MIN_RM_DISTANCE) break;
        if (t > MAX_RM_DISTANCE) break;
    }

    if (t < MAX_RM_DISTANCE) {
        vec3 pos = ro + rd * t;
        vec3 normal = normalsWolverine(pos);
        vec3 viewDir = normalize(ro - pos);
        color = calculateLighting(r.color, normal, lightDir, viewDir, ambientLight);
    }
}
//END WOLVERINE

void mainImage(out vec4 fragColor, in vec2 fragCoord)
{
    vec2 uv = (2.0 * fragCoord - iResolution.xy) / iResolution.y;

    vec3 ro = vec3(0., 0., -3.5);
    vec3 rd = normalize(vec3(uv.x, uv.y, 1.));

    vec3 lightDir = normalize(vec3(-0.4, 0.7, 1.0));
    vec3 ambientLight = vec3(0.2, 0.1, 0.1);

    vec3 col = vec3(0.);
    float t = 0.;

    vec3 colDeadpool = vec3(0.);
    float tDeadpool = 0.;
    vec3 colWolverine = vec3(0.);
    float tWolverine = 0.;

    logoDeadPool(ro, rd, lightDir, ambientLight, colDeadpool, tDeadpool);
    logoWolverine(ro, rd, lightDir, ambientLight, colWolverine, tWolverine);

    if (tDeadpool < tWolverine) {
        col = colDeadpool;
        t = tDeadpool;
    } else {
        col = colWolverine;
        t = tWolverine;
    }

    vec3 bgDeadpool = background(uv, vec3(0.370, 0.004, 0.011), vec3(0.520, 0.100, 0.018));
    vec3 bgWolverine = background(uv, vec3(0.370, 0.30, 0.011), vec3(0.520, 0.420, 0.018));
    float bgT = uv.x * 0.5 + 0.5;
    bgT = smoothstep(0., 1., bgT);
    vec3 bg = mix(bgDeadpool, bgWolverine, bgT);

    col = mix(col, bg, step(MAX_RM_DISTANCE, t));

    fragColor = vec4(col, 1.0);
}
