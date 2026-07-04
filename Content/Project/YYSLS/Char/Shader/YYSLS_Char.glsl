GLSLShader
{
    Properties
    {
        sampler2D _ColorMap = "white"
        sampler2D _AttriMap = "grey"
        sampler2D _normalMap = "normal"
        sampler2D _DetailMap = "normal"
        sampler2D _maskMap = "white"

        float _DetailTile = 10.0;
    }
    SubShader
    {
        Pass
        { 
            cull back
            GLSLPROGRAM
            #include "Core.glsl"
            #include "Lighting.glsl"

            struct V2F
            {
                vec3 NormalWS;
                vec3 TangentWS;
                vec3 BNormalWS;
                vec3 PositionWS;
                vec3 CamVectorWS;
                vec2 uv;
                #ifdef FORWARDRENDER
                    vec4 posMainLight;
                #endif

            };

            #ifdef VERTEX
            out V2F v2f;
            void main()
            {
                gl_Position = ObjectToClipPos(aPosition);
                v2f.NormalWS = ObjectToWorldN(aNormal*2-1);

                v2f.TangentWS = ObjectToWorldN(aTangent.xyz);


                v2f.TangentWS = normalize(v2f.TangentWS-dot(v2f.TangentWS,v2f.NormalWS) * v2f.NormalWS);
                v2f.BNormalWS = cross(v2f.NormalWS,v2f.TangentWS) * aTangent.w;
                v2f.PositionWS = ObjectToWorldPos(aPosition);
                v2f.CamVectorWS = _CameraPosition - v2f.PositionWS;
                v2f.uv = aTexcoord0;


                #ifdef FORWARDRENDER
                    v2f.posMainLight = _MainLightMatrix * GetModelMatrix() * vec4(aPosition,1.0);
                #endif
            }
            #endif


            #ifdef FRAGMENT


            in V2F v2f;



            #ifdef FORWARDRENDER
                out vec4 FragColor;
            #endif

            #ifdef DEFERREDRENDER
                layout (location = 0) out vec4 _AlbdeoAO;
                layout (location = 1) out vec4  _NormalXY;
                layout (location = 2) out vec4  _MRSC;
                layout (location = 3) out vec4  _Flag;
            #endif

            uniform sampler2D _ColorMap;
            uniform sampler2D _AttriMap;
            uniform sampler2D _normalMap;
            uniform sampler2D _DetailMap;
            uniform sampler2D _maskMap;

            uniform float _DetailTile;

            vec3 rgbToHsvCustom(vec3 c)
            {
                float maxC = max(c.r, max(c.g, c.b));
                float minC = min(c.r, min(c.g, c.b));
                float delta = maxC - minC;
                float h = 0.0;
                if (delta > 0.0) {
                    if (maxC == c.r)      h = (c.g - c.b) / delta;
                    else if (maxC == c.g) h = (c.b - c.r) / delta + 2.0;
                    else                  h = (c.r - c.g) / delta + 4.0;
                    h = fract(h / 6.0);
                }
                float s = delta / (maxC + 1e-4);
                float v = maxC;
                return vec3(h, s, v);
            }
            vec3 hsv2rgb(vec3 c)
            {
                vec3 p = abs(fract(c.xxx + vec3(1.0, 2.0/3.0, 1.0/3.0)) * 6.0 - 3.0);
                return c.z * mix(vec3(1.0), clamp(p - 1.0, 0.0, 1.0), c.y);
            } 
            vec3 hsvShift(vec3 rgb, float hueOffset, float satScale)
            {
                vec3 hsv = rgbToHsvCustom(rgb);
                hsv.x = fract(hsv.x + hueOffset);
                hsv.y = clamp(hsv.y * satScale,0,1);
                return hsv2rgb(hsv);
            }

            float remapRoughness(float base, float curve)
            {
                float inv = 1.0 / max(curve, 1e-4);
                return min(pow(base, inv), 1.0);
            }

            vec3 specularGGX(vec3 N, vec3 V, vec3 L, float roughA, float roughB)
            {
                vec3 H = normalize(V + L);
                float NdotH = saturate(dot(N, H));
                float NdotV = saturate(dot(N, V));
                float NdotL = saturate(dot(N, L));
                float VdotH = saturate(dot(V, H));
                float ra = roughA * roughA;
                float rb = roughB * roughB;
                float d1 = ra / max((NdotH * ra - NdotH) * NdotH + 1.0, 1e-4);
                float d2 = rb / max((NdotH * rb - NdotH) * NdotH + 1.0, 1e-4);
                float D = mix(d1 * d1, d2 * d2, 0.85) * 0.3183;
                float vis = 0.25 / max(NdotL * NdotV, 1e-4);
                vec3 F0 = mix(vec3(0.04), vec3(1.0), 0.0);
                vec3 F = F0 + (1.0 - F0) * pow(1.0 - VdotH, 5.0);
                return vec3(D);
                return F * D * vis * NdotL;
            }


            void main() 
            {
                //vectors
                vec3 NormalWS = normalize(v2f.NormalWS);
                vec3 TangentWS = normalize(v2f.TangentWS);
                vec3 BNormalWS = normalize(v2f.BNormalWS);
                vec3 PositionWS = v2f.PositionWS;
                mat3 TBN = mat3(TangentWS, BNormalWS, NormalWS);
                //纹理采样
                vec3 albedo = texture(_ColorMap, v2f.uv).xyz;
                vec3 albedoSq = albedo * albedo;

                vec4 mat35 = texture(_AttriMap,v2f.uv);
                float matMask = ((1.0-mat35.a) > 0.05)? 1.0:0.0;
                float roughBase = 1.0 - mat35.r;
                float metalOverride = mat35.g;
                float aoPack      = mat35.b;
                float hueMix      = mat35.a;


                vec4 mask34       = texture(_maskMap, v2f.uv);
                float detailMask  = smoothstep(0.5, 1.0, mask34.r);
                float detailEdge  = smoothstep(0.0, 0.5, abs(mask34.r - 0.5) - detailMask);
                float clearcoatTrigger = mask34.g;

                vec3 baseColor = albedoSq;

                // ---- 法线 ----
                vec3 normalTS = UnpackNormal(_normalMap,v2f.uv);                
                vec3 normalDetail = UnpackNormal(_DetailMap,v2f.uv * _DetailTile);                




                vec3 finalNormalTS = normalize(vec3(normalTS.xy + normalDetail.xy,normalTS.z * normalDetail.z));
                vec3 pixelNormalWS = TBN * finalNormalTS;
                // ---- 视线 ----
                vec3 CameraVecWS = normalize(v2f.CamVectorWS);

                
                // ---- 粗糙度 / 金属度 ----
                float satPivot = 1;
                float MIN_ROUGH = 0.1;
                float child25 = 0.5;
                float child90 = 0.0;


                float roughCurveA = mix(0.01, 1.0, saturate(satPivot * 2.0));
                float roughCurveB = mix(1.0, 9.9, saturate(satPivot * 2.0 - 1.0));
                float roughCurve  = mix(roughCurveA, roughCurveB, step(0.5, satPivot));
                roughCurve = 1.0 / max(roughCurve, 1e-4);
                float roughDetail = remapRoughness(roughBase, roughCurve);
                roughDetail = mix(roughBase, roughDetail, detailEdge);
                float roughness = max(mix(roughDetail, roughBase, detailMask), MIN_ROUGH);
                roughness -= child25 * (1.0 - detailEdge);
                roughness = max(roughness, MIN_ROUGH);
                float vertexMetal = saturate(min(child90, 2.0));
                float metalness = mix(vertexMetal, metalOverride, matMask);



                #ifdef FORWARDRENDER
                    Surface s;
                    s.normal = NormalWS;
                    s.position = PositionWS;
                    s.posMainLight = v2f.posMainLight;
                    Light mainLight = GetMainLight(s);

                    // ---- 直接光（简化 GGX + 双层 roughness）----
                    vec3 L = mainLight.direction;
                    float NdotL = saturate(dot(pixelNormalWS, L));
                    float roughA = max(roughness * 0.55, MIN_ROUGH);
                    float roughB = max(roughness * 1.15, MIN_ROUGH);
                    vec3 specDirect = specularGGX(pixelNormalWS, CameraVecWS, L, roughA, roughB);
                    vec3 diffBase = baseColor * (1.0 - metalness);
                    // 湿润/积雪对 albedo 的影响
                    // float wetFade = saturate((uGlobal.camPos_fogStart.w - uMat.child85.x) * uMat.child85.z);
                    // float wetLerp = mix(uMat.child85.y, uMat.child85.w, wetFade);
                    // float wetAmt  = vExtra.z * vExtra.z * saturate(uMat.child86.z - 1.0) * 0.7 * wetLerp;  


                    FragColor = vec4(vec3(detailMask ), 1.0);
                #endif


                #ifdef DEFERREDRENDER
                    _AlbdeoAO = vec4(BaseColor,ao);
                    _Flag = vec4(0.0,1,1,1);
                #endif
            }
            #endif
            ENDGLSL
        }
    }
}