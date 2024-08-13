#include "./lib/consts.glsl"
#include "./lib/math.glsl"
#include "./lib/raymarch.glsl"

#define ROT_SPEED iTime / 7.

RaymarchResult eyeDeadpool(vec3 p, float mult) {
    RaymarchResult r;
    return r;
}

RaymarchResult raymarchDeadpool(vec3 p) {
    RaymarchResult r;
    r.distance = MAX_RM_DISTANCE;
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

    vec3 ro = vec3(0., 0., -3.5);
    vec3 rd = normalize(vec3(uv.x, uv.y, 1.));

    vec3 color = vec3(uv, 1.);
    float t = MAX_RM_DISTANCE;

    fragColor = vec4(color, 1.);
}
