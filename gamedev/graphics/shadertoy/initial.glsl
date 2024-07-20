struct Sphere {
    vec3 center;
    float radius;
    vec3 color;
};

struct Light {
    vec3 dir;
};

Light lights[1] = Light[1](
        Light(normalize(vec3(-0.5, -1., 1.)))
    );

const int sphereCount = 3;
Sphere spheres[sphereCount] = Sphere[sphereCount](
        Sphere(vec3(0.6, 0., -0.4), 0.5, vec3(1.0, 0.0, 0.0)),
        Sphere(vec3(-0.0, 0., 0.), 0.5, vec3(0., 1., 1.)),
        Sphere(vec3(-0.0, -510.0, 0.), 500.0, vec3(0., 1., 1.))
    );

struct RayHit {
    vec3 hit;
    vec3 normal;
    int sphereIndex;
};

RayHit traceRay(vec3 rayOrigin, vec3 rayDirection) {
    RayHit hit;
    hit.sphereIndex = -1;

    int closesSphereIndex = -1;
    float closestT = 10000000.;
    for (int i = 0; i < sphereCount; i++) {
        Sphere sphere = spheres[i];

        vec3 rayOriginS = rayOrigin - sphere.center;

        float a = dot(rayDirection, rayDirection);
        float b = 2.0 * dot(rayOriginS, rayDirection);
        float c = dot(rayOriginS, rayOriginS) - sphere.radius * sphere.radius;

        float discriminant = b * b - 4.0 * a * c;
        if (discriminant < 0.) {
            continue;
        }

        float discriminantSqrRoot = sqrt(discriminant);
        float t = (-b - discriminantSqrRoot) / (2. * a);
        if (t < 0.) {
            continue;
        }
        if (t < closestT) {
            closestT = t;
            closesSphereIndex = i;
        }
    }

    if (closesSphereIndex < 0) {
        return hit;
    }

    Sphere sphere = spheres[closesSphereIndex];
    vec3 rayOriginS = rayOrigin - sphere.center;

    hit.sphereIndex = closesSphereIndex;
    hit.hit = rayOriginS + rayDirection * closestT;
    hit.normal = normalize(hit.hit);
    return hit;
}

void raytracingSpheres(out vec4 fragColor, in vec2 uv) {
    vec3 rayOrigin = vec3(0.0, 0.0, -2.0);
    vec3 rayDirection = normalize(vec3(uv, 1.0));

    Light light = lights[0];
    vec3 finalColor = vec3(0.);
    const int MAX_BOUNCES = 3;
    for (int i = 0; i < MAX_BOUNCES; i++) {
        RayHit hit = traceRay(rayOrigin, rayDirection);
        if (hit.sphereIndex < 0) {
            continue;
        }

        float diffuse = max(0., dot(hit.normal, -light.dir));
        Sphere sphere = spheres[hit.sphereIndex];
        finalColor += sphere.color * diffuse;

        //reflect
        if (true) {
            rayOrigin = hit.hit + 0.001 * hit.normal;
            rayDirection = reflect(rayDirection, hit.normal);
        }
    }

    fragColor = vec4(finalColor, 1.);
}

void mainImage(out vec4 fragColor, in vec2 fragCoord)
{
    vec2 uv = fragCoord / iResolution.xy * 2.0 - 1.0;
    uv.x *= iResolution.x / iResolution.y;
    raytracingSpheres(fragColor, uv);
}
