#include "./lib/collision.glsl"

const float c_FOVDegrees = 120.f;
const float c_Pi = 3.1415f;

void render(out vec4 fragColor, in vec2 uv) {
    float cameraDist = 1.f / tan(c_FOVDegrees * 0.5f * c_Pi / 180.f);
    vec3 rayOrigin = vec3(0., 0., -cameraDist);
    vec3 rayTarget = vec3(uv, 1.);
    vec3 rayDir = normalize(rayTarget - rayOrigin);

    SRayHitInfo hit;
    hit.dist = c_superFar;

    vec3 finalColor = vec3(0.);

    if (TestSphereTrace(rayOrigin, rayDir, hit, vec4(-10., 0., 20., 1.))) {
        finalColor += vec3(1.);
    }

    if (TestSphereTrace(rayOrigin, rayDir, hit, vec4(0., 0., 20., 1.))) {
        finalColor += vec3(0.1, 1., 1.);
    }

    if (TestSphereTrace(rayOrigin, rayDir, hit, vec4(10.0f, 0.0f, 20.0f, 1.0f)))
    {
        finalColor += vec3(0.1f, 0.1f, 1.0f);
    }

    fragColor = vec4(finalColor, 1.);
}

void mainImage(out vec4 fragColor, in vec2 fragCoord)
{
    vec2 uv = fragCoord / iResolution.xy * 2.0 - 1.0;
    uv.x *= iResolution.x / iResolution.y;
    render(fragColor, uv);
}
