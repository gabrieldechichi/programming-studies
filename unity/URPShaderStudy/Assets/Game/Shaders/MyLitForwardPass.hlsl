#include "Packages/com.unity.render-pipelines.universal/ShaderLibrary/Lighting.hlsl"

float4 _Color;
TEXTURE2D(_ColorMap);
SAMPLER(sampler_ColorMap);
float4 _ColorMap_ST;
float _Smoothness;

struct Attributes
{
    float3 positionOS : POSITION;
    float3 normalOS : NORMAL;
    float2 uv : TEXCOORD0;
};

struct Varyings
{
    float4 positionCS : SV_POSITION;
    float2 uv : TEXCOORD0;
    float3 positionWS : TEXCOORD1;
    float3 normalWS : TEXCOORD2;
};

Varyings Vertex(Attributes attr)
{
    Varyings v;
    v.positionCS = TransformObjectToHClip(attr.positionOS);
    v.positionWS = TransformObjectToWorld(attr.positionOS);
    v.normalWS = TransformObjectToWorldNormal(attr.normalOS, false);
    v.uv = attr.uv;
    return v;
}

float4 Fragment(Varyings v) : SV_TARGET
{
    SurfaceData surface = (SurfaceData)0;
    surface.albedo = _Color.rgb;
    surface.alpha = _Color.a;
    surface.specular = 1;
    surface.smoothness = _Smoothness;

    InputData input = (InputData)0;
    input.normalWS = normalize(v.normalWS);
    input.positionCS = v.positionCS;
    input.positionWS = v.positionWS;
    input.viewDirectionWS = GetWorldSpaceNormalizeViewDir(v.positionWS);
    input.shadowCoord = TransformWorldToShadowCoord(v.positionWS);
    half4 blinnPhong = UniversalFragmentBlinnPhong(input, surface);

    // float4 tex = SAMPLE_TEXTURE2D(_ColorMap, sampler_ColorMap, v.uv);
    return blinnPhong;
}
