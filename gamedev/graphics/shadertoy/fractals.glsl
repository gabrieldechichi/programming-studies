//https://iquilezles.org/articles/distfunctions2d/
void sdf_1(out vec4 fragColor, in vec2 uv) {
    //sign distance function
    float d = length(uv);
    //maps 0-1 to -0.5-0.5
    d -= 0.5;

    //creates a ring (0.5 - 0 then 0 =-0.5)
    d = abs(d);

    //any value below a is 0, otherwise 1. Sharp edge
    //d = step(0.1, d);

    //if < a 0, if > b 1, otherwise interpolates between 0 and 1
    d = smoothstep(0., 0.2, d);
    fragColor = vec4(d, d, d, 1.);
}

float sdHexagon(in vec2 p, in float r)
{
    const vec3 k = vec3(-0.866025404, 0.5, 0.577350269);
    p = abs(p);
    p -= 2.0 * min(dot(k.xy, p), 0.0) * k.xy;
    p -= vec2(clamp(p.x, -k.z * r, k.z * r), r);
    return length(p) * sign(p.y);
}
void sdf_2(out vec4 fragColor, in vec2 uv) {
    //sign distance function
    float d = sdHexagon(uv, 0.5);
    d = smoothstep(0., 0.2, d);
    fragColor = vec4(d, d, d, 1.);
}

void timePlay_2(out vec4 fragColor, in vec2 uv) {
    //[0,1]
    float d = length(uv);

    float frequency = 8.;
    float speed = 2.;
    d = sin(d * frequency + speed * iTime) / frequency;
    d = abs(d);
    d = smoothstep(0., 0.05, d);

    fragColor = vec4(d, d, d, 1.);
}

void neonColors_3(out vec4 fragColor, in vec2 uv) {
    vec3 color = vec3(2., 1., 3.);

    float d = length(uv);

    float f1 = 8.;
    float speed = 2.;
    d = sin(d * f1 + speed * iTime) / f1;
    d = abs(d);

    //neon colors (inverse function)
    d = 0.02 / d;
    color *= d;

    fragColor = vec4(color, 1.);
}

void neonColors_4(out vec4 fragColor, in vec2 uv) {
    vec3 color = vec3(2., 1., 3.);

    float d = length(uv);

    float f1 = 8.;
    float f2 = 12.;
    float speed = 2.;
    float speed2 = 5.;
    d = sin(d * f1 + speed * iTime) / f1;
    d = cos(d * f2 + speed2 * iTime) / f2;
    d = abs(d);
    //d = smoothstep(0., 0.05, d);
    d = 0.02 / d;
    color *= d;

    fragColor = vec4(color, 1.);
}

//article: https://iquilezles.org/articles/palettes/
//palette web: http://dev.thi.ng/gradients/
vec3 palette(float t) {
    vec3 a = vec3(0.5, 0.5, 0.5);
    vec3 b = vec3(0.5, 0.5, 0.5);
    vec3 c = vec3(1.0, 1.0, 1.0);
    vec3 d = vec3(0.263, 0.416, 0.557);

    return a + b * cos(6.28318 * (c * t + d));
}

void palette_1(out vec4 fragColor, in vec2 uv) {
    float d = length(uv);
    vec3 color = palette(d);
    fragColor = vec4(color, 1.);
}

void frac_1(out vec4 fragColor, in vec2 uv) {
    //frac is < 1, -0.5 centralizes (-0.5, 0.5). multiplier is frequency
    uv = fract(uv * 2.) - 0.5;

    float d = length(uv);
    vec3 color = palette(d);
    fragColor = vec4(color, 1.);
}

void final(out vec4 fragColor, in vec2 uv) {
    vec2 uv0 = uv;

    float speed = 1.;
    vec3 finalColor = vec3(0);

    for (float i = 0.0; i < 4.0; i++) {
        uv = fract(uv * 1.5) - 0.5;
        float d = length(uv) * exp(-length(uv0));
        float f1 = 8.;
        d = sin(d * f1 + speed * iTime) / f1;
        d = abs(d);

        vec3 colorPalette = palette(length(uv0) + i * .4 + iTime * 0.4);
        //neon colors (inverse function)
        d = pow(0.01 / d, 1.2);

        finalColor += colorPalette * d;
    }

    fragColor = vec4(finalColor, 1.);
}

void mainImage(out vec4 fragColor, in vec2 fragCoord)
{
    // -1, 1 range
    vec2 uv = fragCoord / iResolution.xy * 2.0 - 1.0;
    //aspect ratio
    uv.x *= iResolution.x / iResolution.y;
    final(fragColor, uv);
}

