#include "./lib/collision.glsl"
#include "./lib/random.glsl"
#include "./lib/consts.glsl"

#iChannel0 "self"
#iChannel1 "https://as1.ftcdn.net/v2/jpg/07/37/64/60/1000_F_737646047_Y10v9bIrRNwxfuQY4s5R2gqtK6xerxa3.jpg"

const float c_FOVDegrees = 120.f;
const float c_rayPosNormalNudge = 0.01f;
const int c_maxBounces = 8;

#define MULTI_SAMPLE 0

#if MULTI_SAMPLE == 1
const int c_maxRaysPerPixel = 32;
#endif

void traceScene(in vec3 rayPos, in vec3 rayDir, inout SRayHitInfo hitInfo)
{
    // to move the scene around, since we can't move the camera yet
    vec3 sceneTranslation = vec3(0.0f, 0.0f, 10.0f);
    vec4 sceneTranslation4 = vec4(sceneTranslation, 0.0f);

    // back wall
    {
        vec3 A = vec3(-12.6f, -12.6f, 25.0f) + sceneTranslation;
        vec3 B = vec3(12.6f, -12.6f, 25.0f) + sceneTranslation;
        vec3 C = vec3(12.6f, 12.6f, 25.0f) + sceneTranslation;
        vec3 D = vec3(-12.6f, 12.6f, 25.0f) + sceneTranslation;
        if (TestQuadTrace(rayPos, rayDir, hitInfo, A, B, C, D))
        {
            hitInfo.material.albedo = vec3(0.7f, 0.7f, 0.7f);
            hitInfo.material.emissive = vec3(0.0f, 0.0f, 0.0f);
            hitInfo.material.percentSpecular = 0.0f;
            hitInfo.material.roughness = 0.0f;
            hitInfo.material.specularColor = vec3(0.0f, 0.0f, 0.0f);
        }
    }

    // floor
    {
        vec3 A = vec3(-12.6f, -12.45f, 25.0f) + sceneTranslation;
        vec3 B = vec3(12.6f, -12.45f, 25.0f) + sceneTranslation;
        vec3 C = vec3(12.6f, -12.45f, 15.0f) + sceneTranslation;
        vec3 D = vec3(-12.6f, -12.45f, 15.0f) + sceneTranslation;
        if (TestQuadTrace(rayPos, rayDir, hitInfo, A, B, C, D))
        {
            hitInfo.material.albedo = vec3(0.7f, 0.7f, 0.7f);
            hitInfo.material.emissive = vec3(0.0f, 0.0f, 0.0f);
            hitInfo.material.percentSpecular = 0.0f;
            hitInfo.material.roughness = 0.0f;
            hitInfo.material.specularColor = vec3(0.0f, 0.0f, 0.0f);
        }
    }

    // cieling
    {
        vec3 A = vec3(-12.6f, 12.5f, 25.0f) + sceneTranslation;
        vec3 B = vec3(12.6f, 12.5f, 25.0f) + sceneTranslation;
        vec3 C = vec3(12.6f, 12.5f, 15.0f) + sceneTranslation;
        vec3 D = vec3(-12.6f, 12.5f, 15.0f) + sceneTranslation;
        if (TestQuadTrace(rayPos, rayDir, hitInfo, A, B, C, D))
        {
            hitInfo.material.albedo = vec3(0.7f, 0.7f, 0.7f);
            hitInfo.material.emissive = vec3(0.0f, 0.0f, 0.0f);
            hitInfo.material.percentSpecular = 0.0f;
            hitInfo.material.roughness = 0.0f;
            hitInfo.material.specularColor = vec3(0.0f, 0.0f, 0.0f);
        }
    }

    // left wall
    {
        vec3 A = vec3(-12.5f, -12.6f, 25.0f) + sceneTranslation;
        vec3 B = vec3(-12.5f, -12.6f, 15.0f) + sceneTranslation;
        vec3 C = vec3(-12.5f, 12.6f, 15.0f) + sceneTranslation;
        vec3 D = vec3(-12.5f, 12.6f, 25.0f) + sceneTranslation;
        if (TestQuadTrace(rayPos, rayDir, hitInfo, A, B, C, D))
        {
            hitInfo.material.albedo = vec3(0.7f, 0.1f, 0.1f);
            hitInfo.material.emissive = vec3(0.0f, 0.0f, 0.0f);
            hitInfo.material.percentSpecular = 0.0f;
            hitInfo.material.roughness = 0.0f;
            hitInfo.material.specularColor = vec3(0.0f, 0.0f, 0.0f);
        }
    }

    // right wall
    {
        vec3 A = vec3(12.5f, -12.6f, 25.0f) + sceneTranslation;
        vec3 B = vec3(12.5f, -12.6f, 15.0f) + sceneTranslation;
        vec3 C = vec3(12.5f, 12.6f, 15.0f) + sceneTranslation;
        vec3 D = vec3(12.5f, 12.6f, 25.0f) + sceneTranslation;
        if (TestQuadTrace(rayPos, rayDir, hitInfo, A, B, C, D))
        {
            hitInfo.material.albedo = vec3(0.1f, 0.7f, 0.1f);
            hitInfo.material.emissive = vec3(0.0f, 0.0f, 0.0f);
            hitInfo.material.percentSpecular = 0.0f;
            hitInfo.material.roughness = 0.0f;
            hitInfo.material.specularColor = vec3(0.0f, 0.0f, 0.0f);
        }
    }

    // light
    {
        vec3 A = vec3(-5.0f, 12.4f, 22.5f) + sceneTranslation;
        vec3 B = vec3(5.0f, 12.4f, 22.5f) + sceneTranslation;
        vec3 C = vec3(5.0f, 12.4f, 17.5f) + sceneTranslation;
        vec3 D = vec3(-5.0f, 12.4f, 17.5f) + sceneTranslation;
        if (TestQuadTrace(rayPos, rayDir, hitInfo, A, B, C, D))
        {
            hitInfo.material.albedo = vec3(0.0f, 0.0f, 0.0f);
            hitInfo.material.emissive = vec3(1.0f, 0.9f, 0.7f) * 20.0f;
            hitInfo.material.percentSpecular = 0.0f;
            hitInfo.material.roughness = 0.0f;
            hitInfo.material.specularColor = vec3(0.0f, 0.0f, 0.0f);
        }
    }

    if (TestSphereTrace(rayPos, rayDir, hitInfo, vec4(-9.0f, -9.5f, 20.0f, 3.0f) + sceneTranslation4))
    {
        hitInfo.material.albedo = vec3(0.9f, 0.9f, 0.5f);
        hitInfo.material.emissive = vec3(0.0f, 0.0f, 0.0f);
        hitInfo.material.percentSpecular = 0.1f;
        hitInfo.material.roughness = 0.2f;
        hitInfo.material.specularColor = vec3(0.9f, 0.9f, 0.9f);
    }

    if (TestSphereTrace(rayPos, rayDir, hitInfo, vec4(0.0f, -9.5f, 20.0f, 3.0f) + sceneTranslation4))
    {
        hitInfo.material.albedo = vec3(0.9f, 0.5f, 0.9f);
        hitInfo.material.emissive = vec3(0.0f, 0.0f, 0.0f);
        hitInfo.material.percentSpecular = 0.3f;
        hitInfo.material.roughness = 0.2;
        hitInfo.material.specularColor = vec3(0.9f, 0.9f, 0.9f);
    }

    // a ball which has blue diffuse but red specular. an example of a "bad material".
    // a better lighting model wouldn't let you do this sort of thing
    if (TestSphereTrace(rayPos, rayDir, hitInfo, vec4(9.0f, -9.5f, 20.0f, 3.0f) + sceneTranslation4))
    {
        hitInfo.material.albedo = vec3(0.0f, 0.0f, 1.0f);
        hitInfo.material.emissive = vec3(0.0f, 0.0f, 0.0f);
        hitInfo.material.percentSpecular = 0.5f;
        hitInfo.material.roughness = 0.4f;
        hitInfo.material.specularColor = vec3(1.0f, 0.0f, 0.0f);
    }

    // shiny green balls of varying roughnesses
    {
        if (TestSphereTrace(rayPos, rayDir, hitInfo, vec4(-10.0f, 0.0f, 23.0f, 1.75f) + sceneTranslation4))
        {
            hitInfo.material.albedo = vec3(1.0f, 1.0f, 1.0f);
            hitInfo.material.emissive = vec3(0.0f, 0.0f, 0.0f);
            hitInfo.material.percentSpecular = 1.0f;
            hitInfo.material.roughness = 0.0f;
            hitInfo.material.specularColor = vec3(0.3f, 1.0f, 0.3f);
        }

        if (TestSphereTrace(rayPos, rayDir, hitInfo, vec4(-5.0f, 0.0f, 23.0f, 1.75f) + sceneTranslation4))
        {
            hitInfo.material.albedo = vec3(1.0f, 1.0f, 1.0f);
            hitInfo.material.emissive = vec3(0.0f, 0.0f, 0.0f);
            hitInfo.material.percentSpecular = 1.0f;
            hitInfo.material.roughness = 0.25f;
            hitInfo.material.specularColor = vec3(0.3f, 1.0f, 0.3f);
        }

        if (TestSphereTrace(rayPos, rayDir, hitInfo, vec4(0.0f, 0.0f, 23.0f, 1.75f) + sceneTranslation4))
        {
            hitInfo.material.albedo = vec3(1.0f, 1.0f, 1.0f);
            hitInfo.material.emissive = vec3(0.0f, 0.0f, 0.0f);
            hitInfo.material.percentSpecular = 1.0f;
            hitInfo.material.roughness = 0.5f;
            hitInfo.material.specularColor = vec3(0.3f, 1.0f, 0.3f);
        }

        if (TestSphereTrace(rayPos, rayDir, hitInfo, vec4(5.0f, 0.0f, 23.0f, 1.75f) + sceneTranslation4))
        {
            hitInfo.material.albedo = vec3(1.0f, 1.0f, 1.0f);
            hitInfo.material.emissive = vec3(0.0f, 0.0f, 0.0f);
            hitInfo.material.percentSpecular = 1.0f;
            hitInfo.material.roughness = 0.75f;
            hitInfo.material.specularColor = vec3(0.3f, 1.0f, 0.3f);
        }

        if (TestSphereTrace(rayPos, rayDir, hitInfo, vec4(10.0f, 0.0f, 23.0f, 1.75f) + sceneTranslation4))
        {
            hitInfo.material.albedo = vec3(1.0f, 1.0f, 1.0f);
            hitInfo.material.emissive = vec3(0.0f, 0.0f, 0.0f);
            hitInfo.material.percentSpecular = 1.0f;
            hitInfo.material.roughness = 1.0f;
            hitInfo.material.specularColor = vec3(0.3f, 1.0f, 0.3f);
        }
    }
}

vec3 render(in vec2 uv, inout uint seed) {
    float cameraDist = 1.f / tan(c_FOVDegrees * 0.5f * PI / 180.f);
    vec3 rayPos = vec3(0., 0., -cameraDist);
    vec3 rayTarget = vec3(uv, 1.);
    vec3 rayDir = normalize(rayTarget - rayPos);

    vec3 finalColor = vec3(0.);
    vec3 throughput = vec3(1.);

    vec3 skyColor = vec3(0.1, 0.6, 0.8);
    for (int i = 0; i < c_maxBounces; i++) {
        SRayHitInfo hit;
        hit.dist = c_superFar;
        traceScene(rayPos, rayDir, hit);
        //no hit
        if (hit.dist == c_superFar) {
            finalColor += SRGBToLinear(texture(iChannel1, rayDir.xy).rgb) * throughput;
            break;
        }

        rayPos = (rayPos + rayDir * hit.dist) + hit.normal *
                    c_rayPosNormalNudge;
        float doSpecular = RandomValue(seed) < hit.material.percentSpecular ? 1. : 0.;

        vec3 diffuseDir = normalize(hit.normal + RandomDirection(seed));
        vec3 specularDir = reflect(rayDir, hit.normal);
        specularDir = normalize(mix(specularDir, diffuseDir,
                    hit.material.roughness * hit.material.roughness));
        rayDir = mix(diffuseDir, specularDir, doSpecular);

        finalColor += hit.material.emissive * throughput;
        throughput *= mix(hit.material.albedo, hit.material.specularColor,
                doSpecular);

        //Russian roulette
        //As we bounce more (smaller throughput) we increase the chances of the
        //ray stopping early. Survivors get multiplied effect to compensate
        float p = max(throughput.r, max(throughput.g, throughput.b));
        if (RandomValue(seed) > p) {
            break;
        }
        throughput *= 1.0f / p;
    }

    return finalColor;
}

void mainImage(out vec4 fragColor, in vec2 fragCoord)
{
    uint seed = uint(uint(fragCoord.x) * uint(1973) + uint(fragCoord.y) * uint(9277) + uint(iFrame) * uint(26699)) | uint(1);
    vec3 frameColor = vec3(0.);

    //antialiasing

    #if MULTI_SAMPLE == 1
    for (int i = 0; i < c_maxRaysPerPixel; i++) {
        vec2 jitter = vec2(RandomValue(seed), RandomValue(seed)) - 0.5f;
        vec2 uv = ((fragCoord + jitter) / iResolution.xy) * 2.0 - 1.0;
        uv.x *= iResolution.x / iResolution.y;
        frameColor += render(uv, seed);
    }
    frameColor = frameColor / float(c_maxRaysPerPixel);
    #else

    vec2 jitter = vec2(RandomValue(seed), RandomValue(seed)) - 0.5f;
    vec2 uv = ((fragCoord + jitter) / iResolution.xy) * 2.0 - 1.0;
    uv.x *= iResolution.x / iResolution.y;
    frameColor = render(uv, seed);
    #endif

    vec3 lastFrameColor = texture(iChannel0, fragCoord.xy /
                iResolution.xy).rgb;
    frameColor = mix(lastFrameColor, frameColor, 1. / float(iFrame + 1));

    fragColor = vec4(frameColor, 1.);
}
