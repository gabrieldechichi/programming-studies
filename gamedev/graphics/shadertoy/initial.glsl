#include "./lib/collision.glsl"
#include "./lib/random.glsl"
#include "./lib/consts.glsl"

#iChannel0 "./raytracer_2_pass1.glsl"
#iChannel1 "https://as1.ftcdn.net/v2/jpg/07/37/64/60/1000_F_737646047_Y10v9bIrRNwxfuQY4s5R2gqtK6xerxa3.jpg"

const float c_exposure = 0.5f;

void mainImage(out vec4 fragColor, in vec2 fragCoord)
{
    vec3 color = texture(iChannel0, fragCoord / iResolution.xy).rgb;

    // apply exposure (how long the shutter is open)
    color *= c_exposure;

    // convert unbounded HDR color range to SDR color range
    color = ACESFilm(color);

    // convert from linear to sRGB for display
    color = LinearToSRGB(color);

    fragColor = vec4(color, 1.0f);
}
