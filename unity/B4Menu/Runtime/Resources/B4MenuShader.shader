// Vertex-coloured menu shader. Vertex layout: POSITION, NORMAL, COLOR (rgb), TEXCOORD0.x = emissive (0 lit .. 1 emissive).
// URP sub-shader first (Unity 6 default), Built-in RP fallback second. Both support XR single-pass instanced.
Shader "B4/Menu"
{
    Properties
    {
        _Boost ("Emissive boost", Range(0, 2)) = 1
    }

    SubShader
    {
        Tags { "RenderType"="Opaque" "Queue"="Geometry" "RenderPipeline"="UniversalPipeline" }
        Pass
        {
            Name "B4Unlit"
            Cull Back
            ZWrite On
            ZTest LEqual

            HLSLPROGRAM
            #pragma vertex vert
            #pragma fragment frag
            #pragma multi_compile_instancing
            #include "Packages/com.unity.render-pipelines.universal/ShaderLibrary/Core.hlsl"

            CBUFFER_START(UnityPerMaterial)
                float _Boost;
            CBUFFER_END

            struct Attributes
            {
                float3 positionOS : POSITION;
                float3 normalOS   : NORMAL;
                float4 color      : COLOR;
                float  emis       : TEXCOORD0;
                UNITY_VERTEX_INPUT_INSTANCE_ID
            };

            struct Varyings
            {
                float4 positionCS : SV_POSITION;
                float3 normalWS   : TEXCOORD0;
                float4 color      : TEXCOORD1;
                float  emis       : TEXCOORD2;
                UNITY_VERTEX_OUTPUT_STEREO
            };

            Varyings vert(Attributes IN)
            {
                Varyings OUT;
                UNITY_SETUP_INSTANCE_ID(IN);
                UNITY_INITIALIZE_VERTEX_OUTPUT_STEREO(OUT);
                OUT.positionCS = TransformObjectToHClip(IN.positionOS);
                OUT.normalWS = TransformObjectToWorldNormal(IN.normalOS);
                OUT.color = IN.color;
                OUT.emis = IN.emis;
                return OUT;
            }

            half4 frag(Varyings IN) : SV_Target
            {
                UNITY_SETUP_STEREO_EYE_INDEX_POST_VERTEX(IN);
                float3 n = normalize(IN.normalWS);
                float lit = saturate(dot(n, normalize(float3(0.35, 0.75, -0.55))) * 0.5 + 0.5) * 0.9 + 0.1;
                float shade = lerp(lit, _Boost, saturate(IN.emis));
                return half4(IN.color.rgb * shade, 1.0);
            }
            ENDHLSL
        }
    }

    SubShader
    {
        Tags { "RenderType"="Opaque" "Queue"="Geometry" }
        Pass
        {
            Cull Back
            ZWrite On
            ZTest LEqual

            CGPROGRAM
            #pragma vertex vert
            #pragma fragment frag
            #pragma multi_compile_instancing
            #include "UnityCG.cginc"

            float _Boost;

            struct appdata
            {
                float4 vertex : POSITION;
                float3 normal : NORMAL;
                float4 color  : COLOR;
                float  emis   : TEXCOORD0;
                UNITY_VERTEX_INPUT_INSTANCE_ID
            };

            struct v2f
            {
                float4 pos    : SV_POSITION;
                float3 nrm    : TEXCOORD0;
                float4 color  : TEXCOORD1;
                float  emis   : TEXCOORD2;
                UNITY_VERTEX_OUTPUT_STEREO
            };

            v2f vert(appdata v)
            {
                v2f o;
                UNITY_SETUP_INSTANCE_ID(v);
                UNITY_INITIALIZE_OUTPUT(v2f, o);
                UNITY_INITIALIZE_VERTEX_OUTPUT_STEREO(o);
                o.pos = UnityObjectToClipPos(v.vertex);
                o.nrm = UnityObjectToWorldNormal(v.normal);
                o.color = v.color;
                o.emis = v.emis;
                return o;
            }

            fixed4 frag(v2f i) : SV_Target
            {
                float3 n = normalize(i.nrm);
                float lit = saturate(dot(n, normalize(float3(0.35, 0.75, -0.55))) * 0.5 + 0.5) * 0.9 + 0.1;
                float shade = lerp(lit, _Boost, saturate(i.emis));
                return fixed4(i.color.rgb * shade, 1);
            }
            ENDCG
        }
    }
}
