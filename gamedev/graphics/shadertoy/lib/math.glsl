#ifndef H_MATH
#define H_MATH

vec2 rotate2d(vec2 v, float a) {
    mat2 rotation = mat2(vec2(cos(a), sin(a)), vec2(-sin(a), cos(a)));
    return rotation * v;
}

vec3 rotateY(vec3 point, float angle) {
    float x = point.x * cos(angle) + point.z * sin(angle);
    float z = -point.x * sin(angle) + point.z * cos(angle);
    return vec3(x, point.y, z);
}

float rectangle(vec2 uv, vec2 size) {
    float left = size.x * 0.5;
    float up = size.y * 0.5;
    float cx = 1.0 - smoothstep(left, left + 1.5, abs(uv.x));
    float cy = 1.0 - smoothstep(up, up + 1.5, abs(uv.y));
    return (cx * cy);
}

float box(vec3 p, vec3 b) {
    return length(max(abs(p) - b, 0.0));
}

float cylinder(vec3 p, vec2 h) {
    vec2 d = abs(vec2(length(p.xy), p.z)) - h;
    return min(max(d.x, d.y), 0.0) + length(max(d, 0.0));
}

#endif
