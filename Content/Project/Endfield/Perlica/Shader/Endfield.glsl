GLSLShader
{
    Properties
    {
        sampler2D _Color = "white"
        sampler2D _Normal = "normal"
        sampler2D _Mask = "white"
        sampler2D _Attribute = "grey"

        float _roughness = 0.5
        float _metallic = 0.0
    }
    SubShader
    {
        Pass
        { 
            cull off
            GLSLPROGRAM
            #include "Core.glsl"
            #include "Lighting.glsl"


            #define Q_SCALE 0.002

            // 10-bit 无符号 [0,1023] -> 有符号 [-512,511]
            float unpackSnorm10(uint bits)
            {
                float q = float(bits);
                return (q >= 512.0) ? (q - 1024.0) : q;
            }
            // 八面体解码（与 SPIR-V 一致：fold 后保留原 z，再 normalize）
            vec3 decodeOctahedral(vec2 oct)
            {
                float ox = oct.x;
                float oy = oct.y;
                float z = 1.0 - abs(ox) - abs(oy);
                vec2 xy;
                if (z < 0.0) {
                    xy = (vec2(1.0) - abs(oct.yx)) * sign(oct);
                } else {
                    xy = oct;
                }
                return normalize(vec3(xy, z));
            }
            // 由法线构建与 N 正交的切线基 T（对应 SPIR-V 里的 Gram-Schmidt）
            vec3 buildOrthogonalTangent(vec3 n)
            {
                vec3 a = n.yzx - n.zxy;
                float d = dot(a, n);
                return normalize(a - n * d);
            }
            // 从打包 uint 解出 N 和 tangent(vec4)
            void unpackNormalTangent(uint packedNormal, out vec3 N, out vec4 T)
            {
                // 1) 拆字段
                uint qxBits = (packedNormal >>  0) & 0x3FFu;
                uint qyBits = (packedNormal >> 10) & 0x3FFu;
                uint qzBits = (packedNormal >> 20) & 0x3FFu;
                uint wBit   = (packedNormal >> 31) & 0x1u;
                float qx = unpackSnorm10(qxBits);
                float qy = unpackSnorm10(qyBits);
                float qz = unpackSnorm10(qzBits);
                vec2 oct = vec2(qx, qy) * Q_SCALE;
                N = decodeOctahedral(oct);
                // 2) 构建 T/B 基
                vec3 T0 = buildOrthogonalTangent(N);
                vec3 B0 = normalize(cross(N, T0));
                // 3) 用 qz 在 T-B 平面内确定切线方向
                float param = qz * Q_SCALE;
                float s = (param < 0.0) ? -1.0 : 1.0;
                float a = abs(param);
                float u = 1.0 - 2.0 * a;              // _180
                float v = s * (1.0 - abs(u));         // _183
                vec2 uv = normalize(vec2(u, v));      // _185
                vec3 tangentXYZ = normalize(T0 * uv.x + B0 * uv.y);
                float tangentW = float(wBit) * 2.0 - 1.0;
                T = vec4(tangentXYZ, tangentW);
            }


            struct V2F
            {
                vec2 uv;        
                vec3 NormalWS;  
                vec4 TangentWS; 
                vec3 CameraVecWS;
                vec3 PositionWS;

                #ifdef FORWARDRENDER
                    vec4 posMainLight;
                #endif

            };
            
            #ifdef VERTEX
            out V2F v2f;
            void main()
            {
                //解包法线切线
                uint packedNormal = floatBitsToUint(aNormal.x);
                bool isPacked = (packedNormal & 1073741824u) > 0u;
                vec3 N;
                vec4 T;
                if(isPacked)
                {
                    unpackNormalTangent(packedNormal,N,T);
                }
                else
                {
                    N = aNormal;
                    T = aTangent;
                }
                //世界空间位置
                vec3 worldPos = ObjectToWorldPos(aPosition);
                //gl_pos
                gl_Position = WorldToClipPos(worldPos);;


                //法线切线
                v2f.NormalWS = ObjectToWorldN(N);
                vec3 tangentWS = ObjectToWorldN(T.xyz);
                tangentWS = normalize(tangentWS-dot(tangentWS,v2f.NormalWS) * v2f.NormalWS);
                v2f.TangentWS = vec4(tangentWS,T.w);
                v2f.uv = aTexcoord0;
                v2f.CameraVecWS = _CameraPosition - worldPos;
                v2f.PositionWS = worldPos;

                
                #ifdef FORWARDRENDER
                    v2f.posMainLight = _MainLightMatrix * GetModelMatrix() * vec4(aPosition,1.0);
                #endif
            }
            #endif


            #ifdef FRAGMENT


            in V2F v2f;
            uniform sampler2D _Color;
            uniform sampler2D _Normal;
            uniform sampler2D _Mask;
            uniform sampler2D _Attribute;

            uniform float _roughness;
            uniform float _metallic;


            #ifdef FORWARDRENDER
                out vec4 FragColor;
            #endif

            #ifdef DEFERREDRENDER
                layout (location = 0) out vec4 _AlbdeoAO;
                layout (location = 1) out vec4  _NormalXY;
                layout (location = 2) out vec4  _MRSC;
                layout (location = 3) out vec4  _Flag;
            #endif


            void main() 
            {
                //准备向量
                vec3 NormalWS = normalize(v2f.NormalWS);
                vec3 TangentWS = normalize(v2f.TangentWS.xyz);
                vec3 BNormalWS = normalize(cross(v2f.NormalWS,v2f.TangentWS.xyz) * v2f.TangentWS.w);
                vec3 CameraVecWS = normalize(v2f.CameraVecWS);
                mat3 TBN = mat3(TangentWS, BNormalWS, NormalWS);
                //texture sample
                vec3 BaseColor = texture(_Color, v2f.uv).xyz;
                vec3 normalTS = UnpackNormal(_Normal,v2f.uv,true);
                vec4 attri = texture(_Attribute,v2f.uv);
                //
                float faceSign = gl_FrontFacing?1.0:-1.0;
                //apple TBN
                NormalWS = normalize(TBN * normalTS) * faceSign;


                #ifdef FORWARDRENDER
                    Surface s;
                    s.position = v2f.PositionWS;
                    s.posMainLight = v2f.posMainLight;
                    s.basecolor = BaseColor;
                    s.roughness = _roughness;
                    s.metallic = _metallic;
                    s.normal = NormalWS;
                    s.ao = 1.0;

                    vec3 finalColor = CalcAllLight(s,CameraVecWS);

                    FragColor = vec4(vec3(finalColor), 1.0);
                #endif


                #ifdef DEFERREDRENDER
                    _AlbdeoAO = vec4(BaseColor,ao);
                    _NormalXY = vec4(EncodeNormalOct(NormalWS),0,0);
                    _MRSC = vec4(0.0,Roughness,0.5,0.0);
                    _Flag = vec4(_ShadingModel/255.0,1,1,1);
                #endif

            }
            #endif
            ENDGLSL
        }
    }
}