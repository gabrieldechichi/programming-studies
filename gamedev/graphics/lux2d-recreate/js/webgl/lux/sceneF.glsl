precision mediump float;

uniform vec2 obstacle;
varying vec2 screen_pos;

vec2 obstacle_pos_1 = vec2(300, 150);
vec2 obstacle_pos_2 = vec2(200, 350);
vec2 obstacle_pos_3 = vec2(200, 180);
vec2 obstacle_pos_4 = vec2(440, 150);
float obstacle_rad_1 = 400.0;
float obstacle_rad_2 = 900.0;
float obstacle_rad_3 = 1600.0;
float obstacle_rad_4 = 625.0;

// Computes a darkening factor from a ray-circle test
float circle_factor(vec2 obs_pos, vec2 ray_dir, float rad, float depth, float max)
{
    // Perform the ray-circle test
    vec2 offset = obs_pos - screen_pos;
    float b = dot(ray_dir, offset);
    float c = dot(offset, offset) - rad;
    float disc = b*b - c;
    float disr = sqrt(disc);

    // Compute the intersection parameter
    float t = b - disr;
    float factor = clamp(depth*0.035/disr, 0.15, max);

    // Mask out invalid cases
    float mask = step(disc, 0.0) + step(t, 0.0) + step(depth, t);

    return clamp(factor + mask, 0.0, max);
}

// Computes a darkening factor from a ray-scene test
float scene_factor(vec2 light_pos, float dist)
{
    // Generate light depth and the ray normal
    vec2 dir = normalize(light_pos - screen_pos);

    // Compute a darkening factor contribution from each obstace - return early if possible.
    // TODO: variable size + for loop
    float factor = circle_factor(obstacle_pos_1, dir, obstacle_rad_1, dist, 1.0);
    if(factor <= 0.15)
        return factor;

    factor = circle_factor(obstacle_pos_2, dir, obstacle_rad_2, dist, factor);
    if(factor <= 0.15)
        return factor;

    factor = circle_factor(obstacle_pos_3, dir, obstacle_rad_3, dist, factor);
    if(factor <= 0.15)
        return factor;

    factor = circle_factor(obstacle_pos_4, dir, obstacle_rad_4, dist, factor);
    if(factor <= 0.15)
        return factor;

    return factor;
}

// Computes a light bloom amount for a given light position
float light_bloom(vec2 light_pos, float light_rad, float dist)
{
    // Apply an inverse square fall-off model
    return light_rad/(dist+1.0);
}

void main(void)
{
    vec3 color = vec3(0.48, 0.55, 0.57);

    color += clamp(step(mod(screen_pos.x, 45.0),1.0) + step(mod(screen_pos.y, 45.0),1.0), 0.0, 0.055);

    // Comput lighting contribution from each light
    vec2 light_pos = vec2(128.0, 256.0);
    float dist = distance(light_pos, screen_pos);
    color += vec3(1.0, 0.7, 0.4) * scene_factor(light_pos, dist) * light_bloom(light_pos, 12.0, dist);

    light_pos = vec2(450.0, 290.0);
    dist = distance(light_pos, screen_pos);
    color += vec3(0.7, 0.9, 0.5) * scene_factor(light_pos, dist) * light_bloom(light_pos, 7.0, dist);

    light_pos = obstacle;
    dist = distance(light_pos, screen_pos);
    color += vec3(0.7, 0.55, 0.85) * scene_factor(light_pos, dist) * light_bloom(light_pos, 15.0, dist);
    
    // Render the circular caps on the obstacles
    float factor = clamp(21.0 - distance(obstacle_pos_1, screen_pos), 0.0, 1.0) + 
                    clamp(31.0 - distance(obstacle_pos_2, screen_pos), 0.0, 1.0) +
                    clamp(41.0 - distance(obstacle_pos_3, screen_pos), 0.0, 1.0) +
                    clamp(26.0 - distance(obstacle_pos_4, screen_pos), 0.0, 1.0);

    // Return the final color
    gl_FragColor = vec4(mix(color,vec3(0.85,0.87,0.84),factor), 1.0);
}
