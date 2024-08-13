#include "./lib/collision.glsl"
#include "./lib/random.glsl"
#include "./lib/consts.glsl"

void mainImage(out vec4 fragColor, in vec2 fragCoord)
{
    vec2 uv = fragCoord / iResolution.xy * 2.0 - 1.0;
    uv.x *= iResolution.x / iResolution.y;

    // Seed initialization
    uint seed = uint(uint(fragCoord.x) * uint(1973) + uint(fragCoord.y) * uint(9277) + uint(iFrame) * uint(26699)) | uint(1);

    // Generate a random point inside the sphere
    vec3 randPoint = RandomDirection(seed);

    // Map the point to the fragment coordinates
    float pointSize = 1.0; // Adjust the size of the points
    vec3 projectedPoint = vec3(uv, 1.0); // Let's keep a plane at z=0.5

    // Check if the generated random point is near the projected point
    if (length(randPoint.xy - projectedPoint.xy) < pointSize) {
        fragColor = vec4(1.0);
    } else {
        fragColor = vec4(0.0);
    }
}
