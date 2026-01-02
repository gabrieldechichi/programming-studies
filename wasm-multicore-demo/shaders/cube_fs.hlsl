static const float3 LIGHT_DIR = float3(0.5, 0.8, 0.3);
static const float AMBIENT = 0.15;

struct PixelInput {
    float4 position : SV_POSITION;
    float3 world_normal : TEXCOORD0;
    float4 material_color : TEXCOORD1;
};

float4 ps_main(PixelInput input) : SV_TARGET {
    float3 light_dir = normalize(LIGHT_DIR);
    float3 n = normalize(input.world_normal);
    float ndotl = max(dot(n, light_dir), 0.0);
    float diffuse = AMBIENT + (1.0 - AMBIENT) * ndotl;
    return float4(input.material_color.rgb * diffuse, input.material_color.a);
}
