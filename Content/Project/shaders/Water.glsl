GLSLShader
{
    Properties
    {
        float _speed = 0.01
        sampler2D uNoiseRG
        float IOR = 1.333
        vec3 waterColour = 0.1,0.75,0.9
        float DENSITY = 3.5
        vec3 DETAIL_SCALE = 1,1,1
        float DETAIL_HEIGHT = 0
    }
    SubShader
    {
        Pass
        {
            queue transparent
            cull back
            GLSLPROGRAM
            #include "Core.glsl"
            #include "Light.glsl"

            struct V2F
            {
                vec3 NormalWS;
                vec3 PositionWS;
            };

            #ifdef VERTEX
            out V2F v2f;
            void main()
            {
                gl_Position = ObjectToClipPos(aPosition);
                v2f.NormalWS = ObjectToWorldN(aNormal);
                v2f.PositionWS = ObjectToWorldPos(aPosition);
            }
            #endif

            #ifdef FRAGMENT
            #define FOUR_PI (4.0 * PI)
            in V2F v2f;

            out vec4 FragColor;

            uniform float IOR = 1.333;
            const float ETA = 1.0 / IOR;
            const float ETA_REVERSE = IOR;
            const float minDot = 1e-3;
            // 原 shader 参数
            uniform vec3 waterColour = 0.85 * vec3(0.1, 0.75, 0.9);
            const float CLARITY = 0.75;
            uniform float DENSITY = 3.5;
            const float DETAIL_EPSILON = 2e-3;
            
            //表面细节
            uniform float DETAIL_HEIGHT = 0.1;
            uniform vec3 DETAIL_SCALE = vec3(1.0);
            const vec3 BLENDING_SHARPNESS = vec3(4.0);
            uniform float _speed = 0.01;

            uniform sampler2D uNoiseRG;
            layout (binding=10) uniform sampler2D _WaterVolume;
            layout (binding=11) uniform sampler2D _SceneColor;


            float dot_c(vec3 a, vec3 b)
            {
                return max(dot(a, b), minDot);
            }
            vec2 getGradient(vec2 uv)
            {
                float delta = 1e-1;
                uv *= 0.3;
                float data = texture(uNoiseRG, uv).r;
                float gradX = data - texture(uNoiseRG, uv - vec2(delta, 0.0)).r;
                float gradY = data - texture(uNoiseRG, uv - vec2(0.0, delta)).r;
                return vec2(gradX, gradY);
            }


            float getDistortedTexture(vec2 uv)
            {
                float strength = 0.5;
                float time = _speed * _time + texture(uNoiseRG, 0.25 * uv).g;
                float f = fract(time);
                vec2 grad = getGradient(uv);
                vec2 distortion = strength * grad + vec2(0.0, -0.3);
                float distort1 = texture(uNoiseRG, uv + f * distortion).r;
                float distort2 = texture(uNoiseRG, uv + fract(time + 0.5) * distortion).r;
                return (1.0 - length(grad)) *
                    mix(distort1, distort2, abs(1.0 - 2.0 * f));
            }


            float getTriplanarHeight(vec3 position, vec3 normal)
            {
                float xaxis = getDistortedTexture(DETAIL_SCALE.x * position.zy);
                float yaxis = getDistortedTexture(DETAIL_SCALE.y * position.zx);
                float zaxis = getDistortedTexture(DETAIL_SCALE.z * position.xy);
                vec3 blending = abs(normal);
                blending = normalize(max(blending, vec3(0.00001)));
                blending = pow(blending, BLENDING_SHARPNESS);
                blending /= dot(blending, vec3(1.0));
                return dot(vec3(xaxis, yaxis, zaxis), blending);
            }

            void pixarONB(vec3 n, out vec3 b1, out vec3 b2)
            {
                float sign_ = n.z >= 0.0 ? 1.0 : -1.0;
                float a = -1.0 / (sign_ + n.z);
                float b = n.x * n.y * a;
                b1 = vec3(
                    1.0 + sign_ * n.x * n.x * a,
                    sign_ * b,
                    -sign_ * n.x
                );
                b2 = vec3(
                    b,
                    sign_ + n.y * n.y * a,
                    -n.y
                );
            }

            vec3 getDetailExtrusion(vec3 p, vec3 normal)
            {
                // 原 shader 的 getTriplanar 返回 vec3(scalar)，然后 length()
                // 这里等价乘 sqrt(3)
                float detail = DETAIL_HEIGHT * 1.7320508 * getTriplanarHeight(p, normal);
                // 原代码：1 + smoothstep(0.0, -0.5, p.y)
                // 用稳定写法：低处更强
                float lowerMask = 1.0 - smoothstep(-0.5, 0.0, p.y);
                float d = 1.0 + lowerMask;
                return p + d * detail * normal;
            }
            vec3 getDetailNormal(vec3 p, vec3 normal)
            {
                vec3 tangent;
                vec3 bitangent;
                pixarONB(normal, tangent, bitangent);
                tangent = normalize(tangent);
                bitangent = normalize(bitangent);
                vec3 delTangent = vec3(0.0);
                vec3 delBitangent = vec3(0.0);
                for (int i = 0; i < 2; i++)
                {
                    float s = (i == 0) ? 1.0 : -1.0;
                    delTangent += s * getDetailExtrusion(
                        p + s * tangent * DETAIL_EPSILON,
                        normal
                    );
                    delBitangent += s * getDetailExtrusion(
                        p + s * bitangent * DETAIL_EPSILON,
                        normal
                    );
                }
                vec3 n = normalize(cross(delTangent, delBitangent));
                // 防止法线翻面
                if (dot(n, normal) < 0.0)
                    n = -n;
                return n;
            }


        float distributionGGX(vec3 n, vec3 h, float roughness)
        {
            float a2 = roughness * roughness;
            float NdotH = dot_c(n, h);
            return a2 /
                (PI * pow(NdotH * NdotH * (a2 - 1.0) + 1.0, 2.0));
        }
        float geometrySchlick(float cosTheta, float k)
        {
            return cosTheta / (cosTheta * (1.0 - k) + k);
        }
        float smiths(float NdotV, float NdotL, float roughness)
        {
            float k = pow(roughness + 1.0, 2.0) / 8.0;
            return geometrySchlick(NdotV, k) * geometrySchlick(NdotL, k);
        }
        vec3 fresnelSchlick(float cosTheta, vec3 F0)
        {
            return F0 + (1.0 - F0) * pow(1.0 - cosTheta, 5.0);
        }

        vec3 BRDF(vec3 n, vec3 viewDir, vec3 lightDir, vec3 F0, float roughness)
        {
            vec3 h = normalize(viewDir + lightDir);
            float NdotL = dot_c(n, lightDir);
            float NdotV = dot_c(n, viewDir);
            float HdotV = dot_c(h, viewDir);
            float D = distributionGGX(n, h, roughness);
            float G = smiths(NdotV, NdotL, roughness);
            vec3 F = fresnelSchlick(HdotV, F0);
            return D * F * G / max(0.0001, 4.0 * NdotV * NdotL);
        }

        vec3 sampleEnv(vec3 dir)
        {
            vec3 col = textureLod(_prefiltered, normalize(dir), 0.0).rgb;
            return col;
        }

                // ------------------------------------------------------------
        // Volume scattering / absorption
        // ------------------------------------------------------------
        float HenyeyGreenstein(float g, float costh)
        {
            return (1.0 / FOUR_PI) *
                ((1.0 - g * g) /
                pow(1.0 + g * g - 2.0 * g * costh, 1.5));
        }

        vec3 getEnvironmentThroughWater(
            vec3 p,
            vec3 rayDir,       // camera -> surface
            vec3 entryNormal,
            vec3 geoNormal,
            vec2 screenUV,
            vec3 backOutNormal,
            vec3 backPos,
            out vec3 transmittance,
            out vec3 insideDir
        )
        {
            // 进入水体的折射方向
            insideDir = refract(rayDir, entryNormal, ETA);
            // air -> water 一般不会 TIR，但保险
            if (dot(insideDir, insideDir) < 1e-6)
                insideDir = rayDir;
            insideDir = normalize(insideDir);
     
            // 这是沿视线方向的厚度近似
            float viewThickness = length(backPos - p);
            // 用局部 slab 近似修正成折射方向上的路径长度
            float viewCos = max(0.15, abs(dot(rayDir, geoNormal)));
            float refrCos = max(0.15, abs(dot(insideDir, geoNormal)));
            float pathLength = viewThickness * viewCos / refrCos;
            pathLength = clamp(pathLength, 0.0, 10.0);
            // Beer-Lambert absorption
            float d = DENSITY * pathLength;
            transmittance = exp(-d * (1.0 - waterColour));
            // 出水时，GLSL refract 需要 incident side 的 normal。
            // 后表面 mesh normal 是 outward normal，
            // 水内部看到的 normal 应该取反。
            vec3 exitNormal = -backOutNormal;
            vec3 exitDir = refract(insideDir, exitNormal, ETA_REVERSE);
            // water -> air 可能发生全反射
            if (dot(exitDir, exitDir) < 1e-6 ||
                dot(-insideDir, exitNormal) <= 0.66125)
            {
                exitDir = reflect(insideDir, exitNormal);
            }
            vec3 transmitted = sampleEnv(exitDir);
            
            return vec3(transmitted * transmittance);
        }


        void main()
            {
                vec2 uv = gl_FragCoord.xy / vec2(_ScreenWidth, _ScreenHeight);
                vec4 NormalXY= texture(_WaterVolume,uv);

                float backDepth = NormalXY.z;
                vec3 backNormal = DecodeNormalOct(NormalXY.xy);
                vec3 backPos = ReconstructPositionWS(uv,backDepth);

                Surface s;
                Light mainLight = GetMainLight(s);

                vec3 p = v2f.PositionWS;

                vec3 rayDir = normalize(p - _CameraPosition);
                vec3 geoNormal = normalize(normalize(v2f.NormalWS));
                geoNormal = faceforward(geoNormal, rayDir, geoNormal);
                vec3 n = getDetailNormal(p, geoNormal);
                n = faceforward(n, rayDir, n);
                vec3 V = -rayDir;
                vec3 L = normalize(mainLight.direction);
                vec3 F0 = vec3(0.02);
                float roughness = 0.1;

                // 直接光高光
                vec3 directSpec =
                    BRDF(n, V, L, F0, roughness) *
                    mainLight.color *
                    dot_c(n, L);


                vec3 transmittance;
                vec3 insideDir;
                vec3 throughWater = getEnvironmentThroughWater(p,rayDir,n,geoNormal,uv,backNormal,backPos,transmittance,insideDir);

                // 原 shader 低处更浑浊/有泡沫
                float lowMask = 1.0 - smoothstep(-0.5, 0.0, p.y);
                vec3 result = vec3(0.0);
                // 透射环境
                result += (1.0 - lowMask) * CLARITY * throughWater;
                // 体散射
                float mu = dot(insideDir, L);
                float phase = mix(
                    HenyeyGreenstein(-0.3, mu),
                    HenyeyGreenstein(0.85, mu),
                    0.5
                );

                result += CLARITY *  mainLight.color * transmittance * phase;
                // Fresnel 环境反射
                vec3 reflectedDir = reflect(rayDir, n);
                vec3 reflectedCol = sampleEnv(reflectedDir);
                vec3 F = fresnelSchlick(dot_c(n, V), F0);
                result = mix(result, reflectedCol, F);


                // Foam / crest
                float waveHeight = 1.7320508 * getTriplanarHeight(p, n);
                // 高处泡沫更尖锐
                float highMask = smoothstep(-1.3, 0.2, p.y);
                float e = mix(2.0, 16.0, highMask);
                result += lowMask * pow(max(waveHeight, 0.0), e);
                vec3 col = result + directSpec;

                FragColor = vec4(vec3(col), 1.0);
            }
            #endif
            ENDGLSL
        }
    }
}
