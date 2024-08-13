#include "./lib/consts.glsl"
#include "./lib/math.glsl"
#include "./lib/raymarch.glsl"

#define ROT_SPEED iTime / 7.

RaymarchResult eyeDeadpool(vec3 p, float mult) {
    RaymarchResult r;
    p.xy += vec2(-0.55 * mult, -0.1);
    p.x /= 1.2;
    p.y /= 1.1;

    float eye = sdCylinderZ(p, 0.15, 0.2);

    p.xy += vec2(0.22 * mult, -0.2);
    eye = opSubtraction(eye, sdCylinderZ(p, 0.72, 0.3));
    r.distance = eye;
    r.color = vec3(1.);
    return r;
}

RaymarchResult raymarchDeadpool(vec3 p) {
    RaymarchResult r;
    p = rotateY(p, ROT_SPEED);
    float outerCylinder = sdCylinderZ(p, 0.15, 1.2);
    float outerCylinderHole = sdCylinderZ(p, 0.4, 1.0);
    outerCylinder = opSubtraction(outerCylinder, outerCylinderHole);

    float outerBox = sdBox(p, vec3(0.15, 1.2 - 0.01, 0.15));
    outerCylinder = opUnion(outerBox, outerCylinder);

    float innerCylinder = sdCylinderZ(p, 0.1, 1.);
    if (outerCylinder < innerCylinder) {
        r.distance = outerCylinder;
        r.color = vec3(0.7, 0.1, 0.1);
    } else {
        r.distance = innerCylinder;
        r.color = vec3(0.12);
    }

    RaymarchResult eyeRight = eyeDeadpool(p, 1.);
    RaymarchResult eyeLeft = eyeDeadpool(p, -1.);

    if (eyeRight.distance < r.distance) {
        r = eyeRight;
    }
    if (eyeLeft.distance < r.distance) {
        r = eyeLeft;
    }

    float minusBox = sdBox(p - vec3(10., 0., 0.), vec3(10., 10, 3.));
    r.distance = opSubtraction(r.distance, minusBox);

    return r;
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

    r.distance = t;
    return r;
}

void mainImage(out vec4 fragColor, in vec2 fragCoord) {
    vec2 uv = (2.0 * fragCoord - iResolution.xy)
            / iResolution.x;

    vec3 ro = vec3(0., 0., -2.5);
    vec3 rd = normalize(vec3(uv.x, uv.y, 1.));

    vec3 color = vec3(uv, 1.);
    float t = MAX_RM_DISTANCE;

    RaymarchResult lDeadpool = logoDeadpool(ro, rd);
    color = lDeadpool.color;
    t = lDeadpool.distance;

    color *= 2. / t;

    fragColor = vec4(color, 1.);
}
