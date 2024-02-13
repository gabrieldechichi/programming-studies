Shader "Custom/MyUnlit"
{
    Properties
    {
        [Header(Surface options)]
        [MainTexture] _ColorMap("Color", 2D) = "white" {}
        [MainColor] _Color("Tint", Color) = (1, 1, 1, 1)
        _Smoothness("Smoothness", Float) = 0
    }

    SubShader
    {
        Tags
        {
            "RenderPipeline" = "UniversalPipeline"
        }

        Pass
        {
            Name "ForwardLit"
            Tags
            {
                "LightMode" ="UniversalForward"
            }

            HLSLPROGRAM
            #pragma shader_feature_local_fragment _SPECULAR_COLOR
            #pragma multi_compile _ _MAIN_LIGHT_SHADOWS _MAIN_LIGHT_SHADOWS_CASCADE
            #pragma multi_compile_fragment _ _SHADOWS_SOFT
            #define _SPECULAR_COLOR

            #pragma vertex Vertex
            #pragma fragment Fragment

            #include "MyLitForwardPass.hlsl"
            ENDHLSL
        }

        Pass
        {
            // The shadow caster pass, which draws to shadow maps
            Name "ShadowCaster"
            Tags
            {
                "LightMode" = "ShadowCaster"
            }

            ColorMask 0 // No color output, only depth

            HLSLPROGRAM
            #pragma vertex Vertex
            #pragma fragment Fragment

            #include "MyLitShadowCasterPass.hlsl"
            ENDHLSL
        }
    }
}