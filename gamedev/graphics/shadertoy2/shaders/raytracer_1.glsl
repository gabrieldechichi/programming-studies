#iChannel0 "self"

struct Material {
    vec3 albedo;
    float roughness;
};

struct Sphere {
    vec3 center;
    float radius;
    Material material;
};

struct Light {
    vec3 dir;
};

Light lights[1] = Light[1](
        Light(normalize(vec3(-0.5, -1., 1.)))
    );

const int sphereCount = 2;
Sphere spheres[sphereCount] = Sphere[sphereCount](
        Sphere(vec3(0.), 0.5, Material(vec3(1.0, 0.0, 0.0), 0.0)),
        // Sphere(vec3(-0.1, 0., 0.), 0.5, vec3(0.2, 2., 2.)),
        Sphere(vec3(-0.0, -100.4, 0.), 100.0, Material(vec3(0., 1., .0), 0.5))
    );

struct RayHit {
    vec3 hit;
    vec3 normal;
    int sphereIndex;
};

struct Ray {
    vec3 origin;
    vec3 direction;
};

float seed = 0.;
float randf() {
    return fract(sin(seed++) * 43758.5453123);
}

float randfRange(float min, float max) {
    float baseRandom = randf();
    return min + (max - min) * baseRandom;
}

vec3 randomVec3Range(float min, float max) {
    return vec3(
        randfRange(min, max),
        randfRange(min, max),
        randfRange(min, max)
    );
}

RayHit traceRay(Ray ray) {
    RayHit hit;
    hit.sphereIndex = -1;

    int closesSphereIndex = -1;
    float closestT = 10000000.;
    for (int i = 0; i < sphereCount; i++) {
        Sphere sphere = spheres[i];

        vec3 rayOriginS = ray.origin - sphere.center;

        float a = dot(ray.direction, ray.direction);
        float b = 2.0 * dot(rayOriginS, ray.direction);
        float c = dot(rayOriginS, rayOriginS) - sphere.radius * sphere.radius;

        float discriminant = b * b - 4.0 * a * c;
        if (discriminant < 0.) {
            continue;
        }

        float discriminantSqrRoot = sqrt(discriminant);
        float t = (-b - discriminantSqrRoot) / (2. * a);
        if (t > 0. && t < closestT) {
            closestT = t;
            closesSphereIndex = i;
        }
    }

    if (closesSphereIndex < 0) {
        return hit;
    }

    Sphere sphere = spheres[closesSphereIndex];

    hit.sphereIndex = closesSphereIndex;
    hit.hit = ray.origin + ray.direction * closestT;
    hit.normal = normalize(hit.hit - sphere.center);
    return hit;
}

void raytracingSpheres(out vec4 fragColor, in vec2 uv) {
    Ray ray;
    ray.origin = vec3(0.0, 0.0, -2.0);
    ray.direction = normalize(vec3(uv, 1.0));

    Light light = lights[0];
    vec3 finalColor = vec3(0.);
    float multiplier = 1.;
    const vec3 skyColor = vec3(0.2, 0.6, 0.8);

    const int MAX_BOUNCES = 50;
    for (int i = 0; i < MAX_BOUNCES; i++) {
        RayHit hit = traceRay(ray);
        if (hit.sphereIndex < 0) {
            finalColor += skyColor * multiplier;
            break;
        }

        float diffuse = max(0., dot(hit.normal, -light.dir));
        Sphere sphere = spheres[hit.sphereIndex];
        finalColor += sphere.material.albedo * diffuse * multiplier;
        multiplier *= 0.5;

        //always reflecting for now
        if (true) {
            ray.origin = hit.hit + 0.0001 * hit.normal;
            ray.direction = reflect(
                    ray.direction,
                    hit.normal + sphere.material.roughness * randomVec3Range(-0.5, 0.5));
        }
    }

    fragColor = vec4(finalColor, 1.);
}

void mainImage(out vec4 fragColor, in vec2 fragCoord)
{
    seed = iTime + iResolution.y * fragCoord.x / iResolution.x + fragCoord.y / iResolution.y;

    vec2 uv = fragCoord / iResolution.xy * 2.0 - 1.0;
    uv.x *= iResolution.x / iResolution.y;

    vec4 currentFrameColor;
    raytracingSpheres(currentFrameColor, uv);

    // Spatial filtering parameters
    const float filterRange = 2.; // Influence range of the spatial filter
    const float weightSum = 1.0 / (filterRange * 2. + 1.) / (filterRange * 2. + 1.);

    vec4 accumulatedColor = vec4(0.0);
    for (float dx = -filterRange; dx <= filterRange; ++dx)
    {
        for (float dy = -filterRange; dy <= filterRange; ++dy)
        {
            vec2 jitteredUV = uv + vec2(dx, dy) / iResolution.xy;
            vec4 sampleColor = texture(iChannel0, jitteredUV);
            accumulatedColor += sampleColor * weightSum;
        }
    }

    // Mix current frame color with the accumulated color
    vec4 previousFrameColor = texture(iChannel0, fragCoord / iResolution.xy);
    fragColor = mix(previousFrameColor, currentFrameColor, 0.1);
    fragColor = mix(fragColor, accumulatedColor, 0.5);
}
