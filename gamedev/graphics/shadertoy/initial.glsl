#include "./lib/collision.glsl"

const float c_FOVDegrees = 120.f;
const float c_Pi = 3.14159265359f;
const float c_TwoPi = 2. * c_Pi;
const float c_rayPosNormalNudge = 0.01f;
const int c_maxBounces = 5;

//random

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

vec3 RandomUnitVector(inout uint state)
{
    float z = RandomFloat01(state) * 2.0f - 1.0f;
    float a = RandomFloat01(state) * c_TwoPi;
    float r = sqrt(1.0f - z * z);
    float x = r * cos(a);
    float y = r * sin(a);
    return vec3(x, y, z);
}
//

void traceScene(in vec3 rayPos, in vec3 rayDir, inout SRayHitInfo hitInfo) {
    {
        vec3 A = vec3(-15.0f, -15.0f, 22.0f);
        vec3 B = vec3(15.0f, -15.0f, 22.0f);
        vec3 C = vec3(15.0f, 15.0f, 22.0f);
        vec3 D = vec3(-15.0f, 15.0f, 22.0f);
        if (TestQuadTrace(rayPos, rayDir, hitInfo, A, B, C, D))
        {
            hitInfo.albedo = vec3(0.7f, 0.7f, 0.7f);
            hitInfo.emissive = vec3(0.0f, 0.0f, 0.0f);
        }
    }

    if (TestSphereTrace(rayPos, rayDir, hitInfo, vec4(-10.0f, 0.0f, 20.0f, 1.0f)))
    {
        hitInfo.albedo = vec3(1.0f, 0.1f, 0.1f);
        hitInfo.emissive = vec3(0.0f, 0.0f, 0.0f);
    }

    if (TestSphereTrace(rayPos, rayDir, hitInfo, vec4(0.0f, 0.0f, 20.0f, 1.0f)))
    {
        hitInfo.albedo = vec3(0.1f, 1.0f, 0.1f);
        hitInfo.emissive = vec3(0.0f, 0.0f, 0.0f);
    }

    if (TestSphereTrace(rayPos, rayDir, hitInfo, vec4(10.0f, 0.0f, 20.0f, 1.0f)))
    {
        hitInfo.albedo = vec3(0.1f, 0.1f, 1.0f);
        hitInfo.emissive = vec3(0.0f, 0.0f, 0.0f);
    }

    if (TestSphereTrace(rayPos, rayDir, hitInfo, vec4(10.0f, 10.0f, 20.0f, 5.0f)))
    {
        hitInfo.albedo = vec3(0.0f, 0.0f, 0.0f);
        hitInfo.emissive = vec3(1.0f, 0.9f, 0.7f) * 100.0f;
    }
}

void render(out vec4 fragColor, in vec2 uv, inout uint seed) {
    float cameraDist = 1.f / tan(c_FOVDegrees * 0.5f * c_Pi / 180.f);
    vec3 rayPos = vec3(0., 0., -cameraDist);
    vec3 rayTarget = vec3(uv, 1.);
    vec3 rayDir = normalize(rayTarget - rayPos);

    vec3 finalColor = vec3(0.);
    vec3 throughput = vec3(1.);
    for (int i = 0; i < c_maxBounces; i++) {
        SRayHitInfo hit;
        hit.dist = c_superFar;
        traceScene(rayPos, rayDir, hit);
        //no hit
        if (hit.dist == c_superFar) {
            break;
        }

        rayPos = (rayPos + rayDir * hit.dist) + hit.normal *
                    c_rayPosNormalNudge;
        rayDir = normalize(hit.normal + RandomUnitVector(seed));

        finalColor += hit.emissive * throughput;
        throughput *= hit.albedo;
    }

    fragColor = vec4(finalColor, 1.);
}

void mainImage(out vec4 fragColor, in vec2 fragCoord)
{
    vec2 uv = fragCoord / iResolution.xy * 2.0 - 1.0;
    uv.x *= iResolution.x / iResolution.y;

    uint seed = uint(uint(fragCoord.x) * uint(1973) + uint(fragCoord.y) * uint(9277) + uint(iFrame) * uint(26699)) | uint(1);

    render(fragColor, uv, seed);
}
