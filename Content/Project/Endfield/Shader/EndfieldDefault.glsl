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
    }
    SubShader
    {
        Pass
        {
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

            #ifdef VERTEX
            #define Q_SCALE 0.002

            float unpackSnorm10(uint bits)
            {
                float q = float(bits);
                return (q >= 512.0) ? (q - 1024.0) : q;
            }

            vec3 decodeOctahedral(vec2 oct)
            {
                float z = 1.0 - abs(oct.x) - abs(oct.y);
                vec2 xy = (z < 0.0) ? (vec2(1.0) - abs(oct.yx)) * sign(oct) : oct;
                return normalize(vec3(xy, z));
            }

            vec3 buildOrthogonalTangent(vec3 n)
            {
                vec3 a = n.yzx - n.zxy;
                return normalize(a - n * dot(a, n));
            }

            void unpackNormalTangent(uint packedNormal, out vec3 N, out vec4 T)
            {
                uint qxBits = (packedNormal >> 0) & 0x3FFu;
                uint qyBits = (packedNormal >> 10) & 0x3FFu;
                uint qzBits = (packedNormal >> 20) & 0x3FFu;
                uint wBit = (packedNormal >> 31) & 0x1u;

                vec2 oct = vec2(unpackSnorm10(qxBits), unpackSnorm10(qyBits)) * Q_SCALE;
                N = decodeOctahedral(oct);

                vec3 T0 = buildOrthogonalTangent(N);
                vec3 B0 = normalize(cross(N, T0));

                float param = unpackSnorm10(qzBits) * Q_SCALE;
                float s = (param < 0.0) ? -1.0 : 1.0;
                float u = 1.0 - 2.0 * abs(param);
                vec2 tb = normalize(vec2(u, s * (1.0 - abs(u))));

                T = vec4(normalize(T0 * tb.x + B0 * tb.y), float(wBit) * 2.0 - 1.0);
            }

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

            vec3 unpackNormalMapWY(sampler2D normalMap, vec2 uv, float scale)
            {
                vec4 t = texture(normalMap, uv);
                t.w = t.w * t.x;

                vec2 wy = t.wy;
                vec2 oct = wy * 2.0 - 1.0;
                float z = sqrt(max(0.0, 1.0 - dot(oct, oct)));
                return vec3(oct.x * scale, oct.y * scale, z);
            }

            vec2 encodeNormalEndfield(vec3 n)
            {
                n = normalize(n);
                float sum = dot(abs(n), vec3(1.0));
                vec2 xz = n.xz / sum;

                if (n.y <= 0.0)
                    xz = (vec2(1.0) - abs(xz.yx)) * sign(xz);

                return xz * 0.5 + 0.5;
            }

            vec4 packMaterialAttributes(vec2 uv)
            {
                vec4 attri = texture(_Attribute, uv);
                return vec4(_metallic, _roughness, attri.r, attri.g * 0.3333);
            }

            void main()
            {
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
                outGBuffer4 = vec4(baseColor, 1.0);
            }
            #endif
            ENDGLSL
        }
    }
}
