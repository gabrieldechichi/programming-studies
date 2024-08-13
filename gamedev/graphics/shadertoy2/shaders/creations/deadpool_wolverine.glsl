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
#include "./lib/raymarch.glsl"

vec3 background(vec2 uv, vec3 col1, vec3 col2) {
    uv = rotate2d(uv, PI * 0.25);

    vec2 uvx = mod(uv * 150.0, vec2(4.0, 8.0)) - vec2(2.0, 2.0);
    vec2 uvy = mod(uv * 150.0 + vec2(2.0, 4.0), vec2(4.0, 8.0)) - vec2(2.0, 2.0);
    float cx = rectangle(uvx, vec2(1.0, 1.5));
    float cy = rectangle(uvy, vec2(1.5, 1.0));

    float factor = cx + cy;

    vec3 col = mix(col1, col2, factor);
    float vignetteRadius = 0.4;
    float vignette = min(1.0 - length(uv) + vignetteRadius, 1.0);
    return col * vignette;
}

vec3 calculateLighting(vec3 color, vec3 normal, vec3 viewDir)
{
    vec3 lightDir = normalize(vec3(0.4, -0.7, -1.0));

    //diffuse
    float diffuseFactor = max(dot(normal, lightDir), 0.0);
    vec3 diffuse = vec3(diffuseFactor) * color;

    //specular
    vec3 halfDir = normalize(lightDir + viewDir);
    float specularStrength = 1.;
    float shininess = 82.0;
    float specFactor = pow(max(dot(normal, halfDir), 0.0), shininess);
    vec3 specular = specularStrength * specFactor * vec3(1.0); // White specular highlight

    //ambient
    vec3 ambientLight = vec3(0.2, 0.1, 0.1);
    float ambFactor = 0.5 + 0.5 * dot(normal, vec3(0.0, 1.0, 0.0));
    vec3 ambient = ambientLight * ambFactor;

    return diffuse + specular + ambient;
}

RaymarchResult logoBase(vec3 p, vec3 innerCol, vec3 outerCol) {
    RaymarchResult r;
    float outerCylinder = sdCylinderZ(p, 0.15, 1.2);
    float outerCylinderHole = sdCylinderZ(p, 0.4, 1.0);
    outerCylinder = opSubtraction(outerCylinder, outerCylinderHole);

    float outerBox = sdBox(p, vec3(0.15, 1.20 - 0.01, 0.15));
    outerCylinder = opUnion(outerBox, outerCylinder);

    float innerCylinder = sdCylinderZ(p, 0.10, 1.0);

    if (outerCylinder < innerCylinder) {
        r.distance = outerCylinder;
        r.color = outerCol;
    } else {
        r.distance = innerCylinder;
        r.color = innerCol;
    }
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
    p = rotateY(p, iTime);
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

RaymarchResult eyeDeadpool(vec3 p, float mult) {
    RaymarchResult r;

    p.xy += vec2(-0.55 * mult, -0.1);
    p.x /= 1.2;
    p.y /= 1.1;

    float eye = sdCylinderZ(p, 0.15, 0.2);

    p.xy += vec2(0.22 * mult, -0.2);
    eye = opSubtraction(eye, sdCylinderZ(p, 0.72, 0.3));

    r.distance = eye;
    r.color = vec3(1.0);
    return r;
}

RaymarchResult raymarchDeadpool(vec3 p) {
    RaymarchResult r;
    p = rotateY(p, iTime);
    float outerCylinder = sdCylinderZ(p, 0.15, 1.2);
    float outerCylinderHole = sdCylinderZ(p, 0.4, 1.0);
    outerCylinder = opSubtraction(outerCylinder, outerCylinderHole);

    float outerBox = sdBox(p, vec3(0.15, 1.20 - 0.01, 0.15));
    outerCylinder = opUnion(outerBox, outerCylinder);

    float innerCylinder = sdCylinderZ(p, 0.10, 1.0);

    if (outerCylinder < innerCylinder) {
        r.distance = outerCylinder;
        r.color = vec3(0.7, 0.1, 0.1);
    } else {
        r.color = vec3(0.12);
        r.distance = innerCylinder;
    }

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
    return normalize(e.xyy * raymarchDeadpool(pos + e.xyy * eps).distance +
            e.yyx * raymarchDeadpool(pos + e.yyx * eps).distance +
            e.yxy * raymarchDeadpool(pos + e.yxy * eps).distance +
            e.xxx * raymarchDeadpool(pos + e.xxx * eps).distance);
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

RaymarchResult logoDeadpool(in vec3 ro, in vec3 rd)
{
    RaymarchResult r;
    float t = 0.;
    for (int i = 0; i < MAX_RM_IT; i++) {
        vec3 p = ro + rd * t;
        r = raymarchDeadpool(p);
        t += r.distance;
        if (r.distance < MIN_RM_DISTANCE) break;
        if (t > MAX_RM_DISTANCE) break;
    }

    if (t < MAX_RM_DISTANCE) {
        vec3 pos = ro + rd * t;
        vec3 normal = normalsDeadpool(pos);
        vec3 viewDir = normalize(ro - pos);
        r.color = calculateLighting(r.color, normal, viewDir);
    }

    r.distance = t;
    return r;
}

RaymarchResult logoWolverine(in vec3 ro, in vec3 rd)
{
    RaymarchResult r;
    float t = 0.;
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
        r.color = calculateLighting(r.color, normal, viewDir);
    }

    r.distance = t;
    return r;
}

void mainImage(out vec4 fragColor, in vec2 fragCoord) {
    vec2 uv = (2.0 * fragCoord - iResolution.xy)
            / iResolution.x;

    vec3 ro = vec3(0., 0., -3.5);
    vec3 rd = normalize(vec3(uv.x, uv.y, 1.));

    vec3 color = vec3(0.);
    float t = MAX_RM_DISTANCE;

    RaymarchResult lDeadpool = logoDeadpool(ro, rd);
    RaymarchResult lWolverine = logoWolverine(ro, rd);
    if (lDeadpool.distance < lWolverine.distance) {
        color = lDeadpool.color;
        t = lDeadpool.distance;
    } else {
        color = lWolverine.color;
        t = lWolverine.distance;
    }

    vec3 bgDeadpool = background(uv,
            vec3(0.370, 0.004, 0.011), //col1
            vec3(0.520, 0.100, 0.018)); //col2
    vec3 bgWolverine = background(uv,
            vec3(0.370, 0.30, 0.011), //col1
            vec3(0.520, 0.420, 0.018)); //col2

    float bgT = uv.x * 0.5 + 0.5; //[0,1]
    bgT = smoothstep(0., 1., bgT);
    vec3 bg = mix(bgDeadpool, bgWolverine, bgT);

    color = mix(color, bg, step(MAX_RM_DISTANCE, t));

    fragColor = vec4(color, 1.);
}
