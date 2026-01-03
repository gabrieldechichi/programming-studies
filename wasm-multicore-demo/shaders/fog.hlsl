#include "common.hlsl"

static const float3 FOG_COLOR = float3(2.0 / 255.0, 94.0 / 255.0, 131.0 / 255.0);
static const float FOG_DENSITY = 0.007;

float fog_exp2(float distance, float density) {
    float d = density * distance;
    return saturate(exp(-d * d));
}

float3 apply_fog(float3 color, float3 world_position) {
    float dist = length(camera_pos - world_position);
    float fog_factor = fog_exp2(dist, FOG_DENSITY);
    return lerp(FOG_COLOR, color, fog_factor);
}
