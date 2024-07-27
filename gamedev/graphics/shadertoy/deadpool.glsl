const float PI = 3.14159;
const float EPSILON = 0.001;
const float MAX_DISTANCE = 200.0;
const int MAX_ITERATIONS = 250;

const vec3 LIGHT_DIRECTION = vec3(0.0, 1.0, 1.25);

float pulse(float amplitude, float frequency)
{
    return amplitude *
        max(sin(iTime * frequency), 0.0) *
        max(sin(iTime * frequency * 0.5), 0.0) *
        max(cos(iTime * frequency), 0.0);
}

vec2 rotate2d(vec2 st, float a)
{
    mat2 rotation = mat2(vec2(cos(a), sin(a)), vec2(-sin(a), cos(a)));
    return rotation * st;
}

float vignette(vec2 st)
{
    return min(1.0 - length(st) + 0.38, 1.0);
}

float rectangle(vec2 st, vec2 size)
{
    float left = size.x * 0.5;
    float up = size.y * 0.5;

    float cx = 1.0 - smoothstep(left, left + 1.5, abs(st.x));
    float cy = 1.0 - smoothstep(up, up + 1.5, abs(st.y));

    return (cx * cy);
}

vec3 background(vec2 st)
{
    vec2 uv = rotate2d(st, PI * 0.25);

    vec2 uvx = mod(uv * 150.0, vec2(4.0, 8.0)) - vec2(2.0, 2.0);
    vec2 uvy = mod(uv * 150.0 + vec2(2.0, 4.0), vec2(4.0, 8.0)) - vec2(2.0, 2.0);

    float cx = rectangle(uvx, vec2(1.0, 1.5));
    float cy = rectangle(uvy, vec2(1.5, 1.0));

    return mix(vec3(0.370, 0.004, 0.011), vec3(0.520, 0.100, 0.018), cx + cy);
}

float box(vec3 p, vec3 b)
{
    return length(max(abs(p) - b, 0.0));
}

float cylinder(vec3 p, vec2 h)
{
    vec2 d = abs(vec2(length(p.xy), p.z)) - h;
    return min(max(d.x, d.y), 0.0) + length(max(d, 0.0));
}

vec3 rotateY(vec3 point, float angle)
{
    float r = radians(angle);
    float x = point.x * sin(r) - point.z * cos(r);
    float z = point.x * cos(r) + point.z * sin(r);
    return vec3(x, point.y, z);
}

float logoOuterSection(vec3 position)
{
    float outerCylinder = cylinder(position, vec2(2.0, 1.5));
    float innerCylinder = cylinder(position, vec2(2.5, 0.5));
    float centerBox = box(position, vec3(0.35, 2.25, 0.5));

    float ring = max(-outerCylinder, innerCylinder); // Subtract
    float pillaredRing = min(ring, centerBox); // Union

    return pillaredRing;
}

float logoInnerSection(vec3 position)
{
    float outerCylinder = cylinder(position, vec2(2.0, 0.35));
    return outerCylinder;
}

float logoEye(vec3 position)
{
    float innerSize = 1.5 + pulse(20.0, 3.0); // eye blink
    float leftCylinder = cylinder(position, vec2(0.5, 0.4));
    float rightCylinder = cylinder(position - vec3(1.25, 1., 0.0), vec2(innerSize, 6.0));
    return max(-rightCylinder, leftCylinder);
}

float logoEyes(vec3 position)
{
    vec3 p = position;
    float leftEye = logoEye(p + vec3(1.1, 0.0, 0.0));
    p += vec3(-1.1, 0.0, 0.0);
    p.x *= -1.0;
    float rightEye = logoEye(p);
    return min(leftEye, rightEye);
}

float logo(vec3 position, out vec3 material)
{
    vec3 p = rotateY(position, 90.0 + 40.0 * iTime);

    float result = logoInnerSection(p);
    material = vec3(0.130, 0.130, 0.130);

    float outerSection = logoOuterSection(p);
    if (outerSection < result)
    {
        result = outerSection;
        material = vec3(1.000, 0.074, 0.008);
    }

    float leftEye = logoEyes(p);
    if (leftEye < result)
    {
        result = leftEye;
        material = vec3(0.965, 0.965, 0.965);
    }

    return result;
}

vec3 geometry(vec2 screenPosition, vec3 origin, vec3 right, vec3 up, vec3 forward, out float distanze, out int iterations, out vec3 material)
{
    vec3 direction = normalize(screenPosition.x * right + screenPosition.y * up + forward);

    vec3 position = origin;
    float totalDistance = 0.0;
    float d = EPSILON;
    int sectionIndex;

    for (int i = 0; i < MAX_ITERATIONS; i++)
    {
        if (d < EPSILON || totalDistance > MAX_DISTANCE)
        {
            break;
        }

        d = logo(position, material);
        position += direction * d;
        totalDistance += d;

        iterations = i;
    }

    distanze = d;

    return position;
}

vec3 normal(vec3 position)
{
    vec3 epsx = vec3(EPSILON, 0.0, 0.0);
    vec3 epsy = vec3(0.0, EPSILON, 0.0);
    vec3 epsz = vec3(0.0, 0.0, EPSILON);

    vec3 material;

    return normalize(vec3(logo(position + epsx, material) - logo(position - epsx, material),
            logo(position + epsy, material) - logo(position - epsy, material),
            logo(position + epsz, material) - logo(position - epsz, material)));
}

vec4 color(vec3 position, vec3 lightDirection, vec3 cameraOrigin, vec3 material)
{
    vec3 n = normal(position);

    float diffuse = dot(n, lightDirection);
    float ambient = 0.1;

    return vec4(vec3(diffuse + ambient) * material, 1.0);
}

void mainImage(out vec4 fragColor, in vec2 fragCoord)
{
    vec2 st = (2.0 * fragCoord - iResolution.xy) / iResolution.y;

    // st = (fragCoord / iResolution.xy) * 2.0 - 1.0;
    // st.x *= iResolution.x / iResolution.y;

    vec3 cameraOrigin = vec3(0.0, 0.0, 6.0);
    vec3 cameraTarget = vec3(0.0, 0.0, 0.0);
    vec3 up = vec3(0.0, 1.0, 0.0);

    vec3 cameraForward = normalize(cameraTarget - cameraOrigin);
    vec3 cameraRight = normalize(cross(up, cameraForward));
    vec3 cameraUp = cross(cameraForward, cameraRight);

    vec3 lightDirection = normalize(LIGHT_DIRECTION);

    float distanze;
    int iterations;
    vec3 material;

    vec3 position = geometry(st, cameraOrigin, cameraRight, cameraUp, cameraForward, distanze, iterations, material);

    if (distanze < EPSILON)
    {
        fragColor = color(position, lightDirection, cameraOrigin, material);
    }
    else
    {
        fragColor = vec4(background(st) * vignette(st), 1.0);
    }
}
