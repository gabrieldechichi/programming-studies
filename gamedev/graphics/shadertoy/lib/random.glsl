#ifndef H_RAND
#define H_RAND
#include "./consts.glsl"

uint NextRandom(inout uint state)
{
    state = state * uint(747796405) + uint(2891336453);
    uint result = ((state >> ((state >> uint(28)) + uint(4))) ^ state) * uint(277803737);
    result = (result >> 22) ^ result;
    return result;
}

float RandomValue(inout uint state)
{
    return float(NextRandom(state)) / 4294967295.0; // 2^32 - 1
}

// Random value in normal distribution (with mean=0 and sd=1)
float RandomValueNormalDistribution(inout uint state)
{
    // Thanks to https://stackoverflow.com/a/6178290
    float theta = c_TwoPi * RandomValue(state);
    float rho = sqrt(-2. * log(RandomValue(state)));
    return rho * cos(theta);
}

// Calculate a random direction
vec3 RandomDirection(inout uint state)
{
    // Thanks to https://math.stackexchange.com/a/1585996
    float x = RandomValueNormalDistribution(state);
    float y = RandomValueNormalDistribution(state);
    float z = RandomValueNormalDistribution(state);
    return normalize(vec3(x, y, z));
}

vec2 RandomPointInCircle(inout uint rngState)
{
    float angle = RandomValue(rngState) * c_TwoPi;
    vec2 pointOnCircle = vec2(cos(angle), sin(angle));
    return pointOnCircle * sqrt(RandomValue(rngState));
}

#endif
