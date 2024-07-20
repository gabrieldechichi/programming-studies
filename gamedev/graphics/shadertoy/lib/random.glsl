#ifndef H_RAND
#define H_RAND
#include "./consts.glsl"

uint wang_hash(inout uint seed)
{
    seed = uint(seed ^ uint(61)) ^ uint(seed >> uint(16));
    seed *= uint(9);
    seed = seed ^ (seed >> 4);
    seed *= uint(0x27d4eb2d);
    seed = seed ^ (seed >> 15);
    return seed;
}

float RandomFloat01(inout uint state)
{
    return float(wang_hash(state)) / 4294967296.0;
}

float RandomFloatNormalDist(inout uint state) {
    float theta = 2. * c_Pi * RandomFloat01(state);
    float rho = sqrt(-2. * log(RandomFloat01(state)));
    return rho * cos(theta);
}

vec3 RandomUnitVector(inout uint state)
{
    float z = RandomFloat01(state) * 2.0f - 1.0f;
    float a = RandomFloat01(state) * c_TwoPi;
    float r = sqrt(1.0f - z * z);
    float x = r * cos(a);
    float y = r * sin(a);
    return vec3(x, y, z);
}

vec3 RandomUnitVectorNormalDist(inout uint state)
{
    float z = RandomFloatNormalDist(state) * 2.0f - 1.0f;
    float a = RandomFloatNormalDist(state) * c_TwoPi;
    float r = sqrt(1.0f - z * z);
    float x = r * cos(a);
    float y = r * sin(a);
    return vec3(x, y, z);
}

#endif
