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
        Sphere(vec3(0.6, 0., -0.4), 0.5, vec3(1.0, 0.0, 0.0)),
        Sphere(vec3(-0.0, 0., 0.), 0.5, vec3(0., 1., 1.))
    );

void raytracingSpheres(out vec4 fragColor, in vec2 uv) {
    vec3 finalColor = vec3(0.);

    Light light = lights[0];

    int closesSphereIndex = -1;
    float closestT = 10000000.;
    for (int i = 0; i < sphereCount; i++) {
        Sphere sphere = spheres[i];

        vec3 rayOrigin = vec3(0., 0., -2.);
        vec3 rayDirection = vec3(uv, 1.0f);

        rayOrigin -= sphere.center;

        float a = dot(rayDirection, rayDirection);
        float b = 2.0 * dot(rayOrigin, rayDirection);
        float c = dot(rayOrigin, rayOrigin) - sphere.radius * sphere.radius;

        float discriminant = b * b - 4.0 * a * c;
        if (discriminant < 0.) {
            fragColor = vec4(0.2, 0.2, 0.2, 1);
            continue;
        }

        float discriminantSqrRoot = sqrt(discriminant);
        float t = (-b - discriminantSqrRoot) / (2. * a);
        if (t < closestT) {
            closestT = t;
            closesSphereIndex = i;
        }
    }

    if (closesSphereIndex >= 0) {
        Sphere sphere = spheres[closesSphereIndex];
        vec3 rayOrigin = vec3(0., 0., -2.);
        vec3 rayDirection = vec3(uv, 1.0f);
        rayOrigin -= sphere.center;

        vec3 hitPoint = rayOrigin + rayDirection * closestT;
        vec3 normal = normalize(hitPoint);
        float diffuse = max(0., dot(normal, -light.dir));
        finalColor += sphere.color * diffuse;
    }

    fragColor = vec4(finalColor, 1.);
}

void mainImage(out vec4 fragColor, in vec2 fragCoord)
{
    // -1, 1 range
    vec2 uv = fragCoord / iResolution.xy * 2.0 - 1.0;
    //aspect ratio
    uv.x *= iResolution.x / iResolution.y;
    raytracingSpheres(fragColor, uv);
}
