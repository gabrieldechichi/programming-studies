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

const int sphereCount = 2;
Sphere spheres[sphereCount] = Sphere[sphereCount](
        Sphere(vec3(0.7, 0., -0.4), 0.5, vec3(1.0, 0.0, 0.0)),
        // Sphere(vec3(-0.0, 0., 0.), 0.5, vec3(0., 1., 1.)),
        Sphere(vec3(-0.0, -510.0, 0.), 500.0, vec3(0., 1., .0))
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

    const int MAX_BOUNCES = 2;
    for (int i = 0; i < MAX_BOUNCES; i++) {
        RayHit hit = traceRay(ray);
        if (hit.sphereIndex < 0) {
            finalColor += skyColor * multiplier;
            break;
        }

        float diffuse = max(0., dot(hit.normal, -light.dir));
        Sphere sphere = spheres[hit.sphereIndex];
        finalColor += sphere.color * diffuse * multiplier;
        multiplier *= 0.4;

        //always reflecting for now
        if (true) {
            ray.origin = hit.hit + 0.0001 * hit.normal;
            ray.direction = reflect(ray.direction, hit.normal);
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
