GLSLShader
{
    Properties
    {
        sampler2D _Color = "white"
        sampler2D _Normal = "normal"
        sampler2D _Mask = "white"
        sampler2D _Attribute = "grey"

        float _NormalScale = 1.0
        float _roughness = 0.5
        float _metallic = 0.0
        float _DoubleSided = 1.0


        float _OutLineWidth = 0.01
        vec3 _OutLineColor = (0,0,0)
    }
    SubShader
    {
        Pass
        {
            Tags
            {
                LightMode Deferred
            }
            Stencil 
            {
                BitMask 0xff
                AndMask 0xff
                Func always
                Ref 0x24
                fail keep
                dpfail keep 
                dppass replace
            }
            GLSLPROGRAM
            #include "Core.glsl"
            #include "EndfieldLibrary.glsl"
            #ifdef VERTEX
            layout(location = 0) out vec2 v_uv;
            layout(location = 2) out vec3 v_normalWS;
            layout(location = 3) out vec4 v_tangentWS;
            void main()
            {
                uint packedNormal = floatBitsToUint(aNormal.x);
                bool isPacked = (packedNormal & 0x40000000u) > 0u;

                vec3 N;
                vec4 T;
                if (isPacked)
                    unpackNormalTangent(packedNormal, N, T);
                else
                {
                    N = aNormal;
                    T = aTangent;
                }

                vec3 worldPos = ObjectToWorldPos(aPosition);
                gl_Position = WorldToClipPos(worldPos);

                v_normalWS = ObjectToWorldN(N);
                vec3 tangentWS = ObjectToWorldN(T.xyz);
                tangentWS = normalize(tangentWS - dot(tangentWS, v_normalWS) * v_normalWS);
                v_tangentWS = vec4(tangentWS, T.w);
                v_uv = aTexcoord0;
            }
            #endif

            #ifdef FRAGMENT
            layout(location = 0) in vec2 v_uv;
            layout(location = 2) in vec3 v_normalWS;
            layout(location = 3) in vec4 v_tangentWS;

            uniform sampler2D _Color;
            uniform sampler2D _Normal;
            uniform sampler2D _Mask;
            uniform sampler2D _Attribute;

            uniform float _NormalScale;
            uniform float _roughness;
            uniform float _metallic;
            uniform float _DoubleSided;

            layout(location = 0) out vec4 outGBuffer0;
            layout(location = 1) out vec4 outGBuffer1;
            layout(location = 2) out vec4 outGBuffer2;
            layout(location = 3) out vec4 outGBuffer3;
            layout(location = 4) out vec4 outGBuffer4;

            vec4 packMaterialAttributes(vec2 uv)
            {
                vec4 attri = texture(_Attribute, uv);
                return vec4(_metallic, _roughness, attri.r, attri.g * 0.3333);
            }
            void main()
            {
                vec4 mask = texture(_Mask,v_uv);
                if (mask.x < 0.3)
                    discard;


                vec3 N = normalize(v_normalWS);
                vec3 T = normalize(v_tangentWS.xyz);
                vec3 B = cross(N, T) * v_tangentWS.w;
                mat3 TBN = mat3(T, B, N);

                vec3 normalTS = unpackNormalMapWY(_Normal, v_uv, _NormalScale);
                vec3 normalWS = normalize(TBN * normalTS);

                float backFaceSign = -1.0 + 2.0 * _DoubleSided;
                normalWS *= gl_FrontFacing ? 1.0 : backFaceSign;

                vec3 baseColor = texture(_Color, v_uv).rgb;
                vec2 normalEnc = encodeNormalEndfield(normalWS);

                outGBuffer0 = vec4(0.0);
                outGBuffer1 = vec4(0.0, 0.0, 1.0, 0.4);
                outGBuffer2 = packMaterialAttributes(v_uv);
                outGBuffer3 = vec4(normalEnc, 0.0, 0.0);
                outGBuffer4 = vec4(baseColor,1.0);
            }
            #endif
            ENDGLSL
        }

        Pass
        {
            Tags
            {
                LightMode Outline
            }
            Stencil 
            {
                BitMask 0xff
                AndMask 0xff
                Func notequal
                Ref 0x24
                fail keep
                dpfail keep 
                dppass keep
            }
            cull front
            GLSLPROGRAM
            #include "Core.glsl"
            #include "EndfieldLibrary.glsl"

            uniform float _OutLineWidth;
            #ifdef VERTEX
            layout(location = 2) out vec3 v_normalWS;
            layout(location = 3) out vec4 v_tangentWS;
            void main()
            {
                uint packedNormal = floatBitsToUint(aNormal.x);
                bool isPacked = (packedNormal & 0x40000000u) > 0u;
                vec3 N;
                vec4 T;
                if (isPacked)
                    unpackNormalTangent(packedNormal, N, T);
                else
                {
                    N = aNormal;
                    T = aTangent;
                }
                vec3 localPos = aPosition;
                localPos += N * _OutLineWidth;
                vec3 worldPos = ObjectToWorldPos(localPos);
                gl_Position = WorldToClipPos(worldPos);

            }
            #endif

            #ifdef FRAGMENT
            uniform vec3 _OutLineColor;

            layout(location = 4) out vec4 outGBuffer4;

            void main()
            {
                outGBuffer4 = vec4(_OutLineColor,1.0);
            }
            #endif
            ENDGLSL
        }
    }
}
