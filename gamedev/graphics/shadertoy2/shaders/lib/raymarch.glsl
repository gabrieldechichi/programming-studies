#ifndef H_RAYMARCHING
#define H_RAYMARCHING

#define MAX_RM_DISTANCE 200.0
#define MIN_RM_DISTANCE EPSILON
#define MAX_RM_IT 100

#include "./math.glsl"

struct RaymarchResult {
    float distance;
    vec3 color;
};

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

float sdSphere(vec3 p, float radius) {
    return length(p) - radius;
}

float sdBox(vec3 p, vec3 b)
{
    vec3 q = abs(p) - b;
    return length(max(q, 0.0)) + min(max(q.x, max(q.y, q.z)), 0.0);
}
#endif
