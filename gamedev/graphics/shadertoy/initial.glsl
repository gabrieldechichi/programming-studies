struct Sphere {
    vec2 center;
    float radius;
};

struct Light {
    vec3 dir;
};

Sphere spheres[1];

void raytracingSpheres(out vec4 fragColor, in vec2 uv) {
    Light light;
    light.dir = normalize(vec3(-0.5, -1., 1.));

    spheres[0].center = vec2(0., 0.);
    spheres[0].radius = 0.5f;
    Sphere sphere = spheres[0];

    vec3 rayOrigin = vec3(0., 0., -2.);
    vec3 rayDirection = vec3(uv, 1.0f);

    rayOrigin.xy -= sphere.center;

    float a = dot(rayDirection, rayDirection);
    float b = 2.0 * dot(rayOrigin, rayDirection);
    float c = dot(rayOrigin, rayOrigin) - sphere.radius * sphere.radius;

    float discriminant = b * b - 4.0 * a * c;
    if (discriminant < 0.) {
        fragColor = vec4(0.2, 0.2, 0.2, 1);
        return;
    }

    float discriminantSqrRoot = sqrt(discriminant);
    float t = (-b - discriminantSqrRoot) / (2. * a);

    vec3 hitPoint = rayOrigin + rayDirection * t;
    vec3 normal = normalize(hitPoint);
    normal *= 0.5 + 0.5;

    float diffuse = dot(normal, -light.dir);

    vec3 color = vec3(1.);
    color *= diffuse;
    fragColor = vec4(color, 1.);

    // float discriminantSqroot = std::sqrt(discriminant);
    // // a is always positive so -b - discriminantSqroot is the closest t
    // float t = (-b - discriminantSqroot) / (2 * a);
    //
    // auto hitPoint = rayOrigin + rayDirection * t;
    // auto normal = glm::normalize(hitPoint);
    // // normal = normal * 0.5f + 0.5f;
    // auto diffuse = glm::max(0.0f, glm::dot(normal, -light.dir));
    // auto color = glm::vec3(1, 0, 1);
    // color *= diffuse;
    //
    // return glm::vec4(color, 1);
}

void mainImage(out vec4 fragColor, in vec2 fragCoord)
{
    // -1, 1 range
    vec2 uv = fragCoord / iResolution.xy * 2.0 - 1.0;
    //aspect ratio
    uv.x *= iResolution.x / iResolution.y;
    raytracingSpheres(fragColor, uv);
}
