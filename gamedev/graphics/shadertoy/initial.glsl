#include "./lib/consts.glsl"
#include "./lib/math.glsl"
#include "./lib/raymarch.glsl"
#include "./lib/simplexnoise.glsl"

vec3 calculateLighting(vec3 color, vec3 normal, vec3 viewDir)
{
    vec3 lightDir = normalize(vec3(0.4, 0.7, -1.0));

    //diffuse
    float diffuseFactor = max(dot(normal, lightDir), 0.0);
    vec3 diffuse = vec3(diffuseFactor) * color;

    //specular
    vec3 halfDir = normalize(lightDir + viewDir);
    float specularStrength = 1.;
    float shininess = 82.0;
    float specFactor = pow(max(dot(normal, halfDir), 0.0), shininess);
    vec3 specular = specularStrength * specFactor * vec3(1.0); // White specular highlight

    //ambient
    vec3 ambientLight = vec3(0.2, 0.1, 0.1);
    float ambFactor = 0.5 + 0.5 * dot(normal, vec3(0.0, 1.0, 0.0));
    vec3 ambient = ambientLight * ambFactor;

    return diffuse + specular + ambient;
}

RaymarchResult map(vec3 p) {
    RaymarchResult r;

    float maxDist = 2.;
    float freq = 1.2;
    vec3 pBox = p;
    vec3 pSphere = p + vec3(sin(iTime * freq) * maxDist, 0., 0.);
    vec3 pTri = p + vec3(0., -sin(iTime * freq) * maxDist, 0.);

    float sphere = sdSphere(pSphere, 0.4);
    float triangle = sdTriPrismZ(pTri, vec2(0.5, 0.4));
    float box = sdBox(p, vec3(0.4));
    float u = opSmoothUnion(triangle, opSmoothUnion(sphere, box, 0.3), 0.3);

    vec3 colSphere = vec3(1.0, 0.0, 0.0); // Red color for sphere
    vec3 colTri = vec3(0.0, 1.0, 0.0);    // Green color for triangle
    vec3 colBox = vec3(0.0, 0.0, 1.0);    // Blue color for box

    r.distance = u;
    
    if (abs(u - sphere) < EPSILON) {
        r.color = colSphere;
    } else if (abs(u - triangle) < EPSILON) {
        r.color = colTri;
    } else {
        r.color = colBox;
    }

    return r;
}

vec3 normals(in vec3 pos)
{
    vec2 e = vec2(1.0, -1.0) * 0.5773;
    const float eps = EPSILON;
    return normalize(e.xyy * map(pos + e.xyy * eps).distance +
            e.yyx * map(pos + e.yyx * eps).distance +
            e.yxy * map(pos + e.yxy * eps).distance +
            e.xxx * map(pos + e.xxx * eps).distance);
}

void mainImage(out vec4 fragColor, in vec2 fragCoord) {
    vec2 uv = (2.0 * fragCoord - iResolution.xy)
            / iResolution.y;

    vec3 ro = vec3(0., 0., -2.5);
    vec3 rd = normalize(vec3(uv.x, uv.y, 1.));

    RaymarchResult r;
    float t = 0.;
    vec3 color = vec3(0.);
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
        vec3 viewDir = normalize(ro - pos);
        r.color = calculateLighting(
            r.color,
            normal,
            viewDir);
    }

    color = r.color * 10. / pow(t,3.);

    fragColor = vec4(color, 1.);
}
