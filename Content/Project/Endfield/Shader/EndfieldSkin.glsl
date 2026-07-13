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
        float _NormalViewBlend = 0.0
        float _RimStrength = 1.0
        float _TextureBias = 0.0
        vec4 _ColorTint = "1,1,1,1"
        vec4 _ColorBlend = "1,1,1,1"
        vec4 _UvTransform = "1,1,0,0"
        vec4 _ClipScale = "0,0,0,0"
    }
    SubShader
    {
        Pass
        {
            Tags
            {
                RenderPipeline Endfield
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

            uniform vec4 _UvTransform;
            uniform vec4 _ClipScale;

            #ifdef VERTEX
            layout(location = 0) out vec2 v_uv;
            layout(location = 1) out vec3 v_worldPos;
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
                vec4 clipPos = GL_MATRIX_P * GL_MATRIX_V * vec4(worldPos, 1.0);
                gl_Position = clipPos;
                v_uv = aTexcoord0 * _UvTransform.xy + _UvTransform.zw;
                v_worldPos = worldPos;
                v_normalWS = ObjectToWorldN(N);
                vec3 tangentWS = ObjectToWorldN(T.xyz);
                tangentWS = normalize(tangentWS - dot(tangentWS, v_normalWS) * v_normalWS);
                v_tangentWS = vec4(tangentWS, T.w);
            }
            #endif

            #ifdef FRAGMENT
            layout(location = 0) in vec2 v_uv;
            layout(location = 1) in vec3 v_worldPos;
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
            uniform float _NormalViewBlend;
            uniform float _RimStrength;
            uniform float _TextureBias;
            uniform vec4 _ColorTint;
            uniform vec4 _ColorBlend;

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
                vec3 viewDir = normalize(_CameraPosition - v_worldPos);
                vec3 cameraBasisN = normalize(mat3(GL_MATRIX_V)[2]);
                vec3 shadingNormal = normalize(mix(viewDir, cameraBasisN, _NormalViewBlend));

                vec3 baseColor = texture(_Color, v_uv, _TextureBias).rgb * _ColorTint.rgb;

                vec3 geomNormal = normalize(v_normalWS);
                vec3 T = normalize(v_tangentWS.xyz);
                vec3 B = cross(geomNormal, T) * v_tangentWS.w;
                mat3 TBN = mat3(T, B, geomNormal);
                vec3 normalTS = unpackNormalMapWY(_Normal, v_uv, _NormalScale);
                vec3 normalWS = normalize(TBN * normalTS);

                float backFaceSign = -1.0 + 2.0 * _DoubleSided;
                normalWS *= gl_FrontFacing ? 1.0 : backFaceSign;

                float ndl = clamp(dot(normalWS, shadingNormal) * 0.85 + 0.15, 0.0, 1.0);
                float shade = clamp((1.0 - ndl) * _RimStrength, 0.0, 1.0);
                vec3 shadeColor = mix(vec3(1.0 - shade), _ColorBlend.rgb, shade);
                vec3 finalColor = baseColor * shadeColor;

                vec2 normalEnc = encodeNormalEndfield(normalWS);

                outGBuffer0 = vec4(0.0);
                outGBuffer1 = vec4(0.0, 0.0, 1.0, 0.4);
                outGBuffer2 = packMaterialAttributes(v_uv);
                outGBuffer3 = vec4(normalEnc, 0.0, 0.4);
                outGBuffer4 = vec4(finalColor, 1.0);
            }
            #endif
            ENDGLSL
        }
    }
}
