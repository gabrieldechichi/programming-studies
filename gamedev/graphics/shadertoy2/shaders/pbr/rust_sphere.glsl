#include "./lib/consts.glsl"
#include "./lib/math.glsl"
#include "./lib/simplexnoise.glsl"

const float MAX_RM_DISTANCE = 200.0;
const float MIN_RM_DISTANCE = EPSILON;
const int MAX_RM_IT = 100;

float sdSphere(vec3 p, float radius) {
    return length(p) - radius;
}

struct RaymarchResult {
    float distance;
    vec3 color;
};

RaymarchResult map(vec3 p) {
    p = rotateY(p, iTime);
    RaymarchResult r;
    r.distance = sdSphere(p, 0.5);

    // Layered noise for rust effect
    float noise1 = simplexNoise(p * 3.0);
    float noise2 = simplexNoise(p * 10.0);

    // Adding higher-frequency noise for edge irregularity
    float highFreqNoise = simplexNoise(p * 50.0) * 0.3; // Adjust scaling factor as needed

    float rustFactor = smoothstep(0.0, 0.5, mix(noise1, noise2, 0.5) + highFreqNoise);

    vec3 metalColor = vec3(0.8, 0.8, 0.8);
    vec3 rustColor = metalColor * 0.3;
    rustColor.r += 0.1;
    r.color = mix(metalColor, rustColor, rustFactor);

    return r;
}

vec3 normals(in vec3 pos) {
    vec2 e = vec2(1.0, -1.0) * 0.5773;
    const float eps = EPSILON;
    return normalize(e.xyy * map(pos + e.xyy * eps).distance +
            e.yyx * map(pos + e.yyx * eps).distance +
            e.yxy * map(pos + e.yxy * eps).distance +
            e.xxx * map(pos + e.xxx * eps).distance);
}

// Surface Distance Function

void mainImage(out vec4 fragColor, in vec2 fragCoord) {
    vec2 uv = (2.0 * fragCoord - iResolution.xy) / iResolution.y;

    vec3 ro = vec3(0., 0., -3.5);
    vec3 rd = normalize(vec3(uv.x, uv.y, 1.));

    vec3 lightDir = normalize(vec3(1.0, 1.0, -1.0));
    vec3 ambientLight = vec3(0.3, 0.3, 0.3);

    vec3 col = vec3(0.1, 0.7, 0.7);
    float t = 0.;

    RaymarchResult r;
    for (int i = 0; i < MAX_RM_IT; i++) {
        vec3 p = ro + rd * t;
        r = map(p);
        t += r.distance;
        if (r.distance < MIN_RM_DISTANCE) break;
        if (t > MAX_RM_DISTANCE) break;
    }

    if (t < MAX_RM_DISTANCE) {
        vec3 pos = ro + rd * t;
        vec3 normal = normals(pos);

        // Phong reflection model
        vec3 viewDir = normalize(ro - pos);
        vec3 halfDir = normalize(lightDir + viewDir);
        float specularStrength = 0.5;

        float shininess = 32.0; // Lower value for rougher surface
        float specFactor = pow(max(dot(normal, halfDir), 0.0), shininess);
        vec3 specular = specularStrength * specFactor * vec3(1.0); // White specular highlight

        float diffuse = max(dot(normal, lightDir), 0.0);
        float ambFactor = 0.5 + 0.5 * dot(normal, -vec3(1.0, 1.0, 0.0));
        vec3 diffuseColor = vec3(diffuse) * r.color;

        col = diffuseColor + ambientLight * ambFactor + specular;
    }

    fragColor = vec4(col, 1.0);
}
