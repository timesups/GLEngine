GLSLShader
{
    Properties
    {
        sampler2D _baseColor = "HuLi_Color.png"
        vec4 _BaseColor0 = vec4(1.0, 1.0, 1.0, 0.0)
        float _RimIntensity = 0.96
        vec4 _RimColor = vec4(1.0, 0.8, 0.6, 1.0)
        float _RimStart = 0.2
        float _RimEnd = 0.8
        float _RimInt = 1.0
        float _FurStepStrength = 0.8


        sampler2D _furMap = "Fur_map.png"
        float _Thinness = 25
        float _ThinnessScale = 1.5
        float _FurLength = 0.05
        float _FurLengthScale = 0.46


        sampler2D _FurDirectionMap = "Fur_Direction.png"
        vec3 _GForce = vec3(0.0, -0.02, 0.0)
        float _WindSpeed = 1.0
        vec3 _LForce = vec3(0.02, 0.0, 0.0)
        vec3 _FurOffset = vec3(0.02,0.02,0.0)
        float _AdaptiveScale = 0.36

        float _LayerEdgeSmoothnessStrength = 1.03

        float _FurAlpha = 1.0
        float _FurClip = 0.19



        bool _EnablePbrLight = false
    }
    SubShader
    {
        Pass
        {
            DrawTime 15
            queue Transparent
            ztest lequal
            cull off
            blend srcalpha oneminussrcalpha
            GLSLPROGRAM
            #include "Core.glsl"
            #include "Lighting.glsl"

            uniform sampler2D _FurDirectionMap;
            uniform sampler2D _baseColor;
            uniform sampler2D _furMap;

            uniform float _FurLength;
            uniform float _FurLengthScale;
            uniform vec3 _FurOffset;
            uniform float _FurStepStrength;
            uniform float _AdaptiveScale;
            uniform vec3 _GForce;
            uniform vec3 _LForce;
            uniform float _WindSpeed;
            uniform float _RimIntensity;
            uniform vec4 _RimColor;
            uniform float _RimStart;
            uniform float _RimEnd;
            uniform float _RimInt;
            uniform float _LayerEdgeSmoothnessStrength;
            uniform float _FurAlpha;
            uniform float _Thinness;
            uniform float _ThinnessScale;
            uniform float _FurClip;
            uniform vec4 _BaseColor0;


            uniform bool _EnablePbrLight;

            struct V2F
            {
                vec4 uv;
                vec3 worldNormal;
                vec3 viewDir;
                vec4 furDirection;
                vec3 worldPos;
                vec3 vertexLighting;
                vec4 rim;
                float layerEdgeSmooth;
            };

            float FurSmoothStep01(float t)
            {
                t = clamp(t, 0.0, 1.0);
                return t * t * (3.0 - 2.0 * t);
            }

            float EffectiveFurLayerScale()
            {
                return float(_PassIndex)/float(_DrawCount);
            }
            #ifdef VERTEX

            vec3 ComputeWindBlendedNormal(vec3 normal, out float normalLen)
            {
                float phase = _time * _WindSpeed;
                float windFactor = (sin(phase) * cos(phase * 0.666666687) + 1.0) * 0.5;
                vec3 windOffset = windFactor * _LForce;
                normalLen = length(normal);
                vec3 nA = normalize(normal + windOffset);
                return mix(nA, nA * normalLen - nA, _AdaptiveScale);
            }

            vec3 ComputeFurMainLight(vec3 worldNormal, float furDirZ, float furLayer)
            {
                vec3 lightDir = normalize(-_MainLightDirection);
                float ndotl = dot(worldNormal, lightDir);
                float stepBase = clamp(ndotl, 0.0, 1.0);
                vec3 lightColor = _MainLightColor;

                vec3 term0 = (1.0 - furDirZ) * stepBase * lightColor;

                float layerStep = clamp(furLayer * _FurStepStrength + ndotl + 0.2, 0.0, 1.0);
                vec3 term1 = layerStep * lightColor;
                float layerStep2 = clamp(furLayer * _FurStepStrength + 0.7, 0.0, 1.0);
                term1 *= layerStep2;
                term1 *= furDirZ;
                term0 = term1 + term0;
                return term0;
            }

            void ComputeFurRim(vec3 worldNormal, vec3 viewDir, vec3 ambientLight, float furLayer, out vec3 rimAdd, out vec4 rimOut)
            {
                float ndotV = clamp(dot(worldNormal, viewDir), 0.0, 1.0);
                float invNdotV = 1.0 - ndotV;
                float layerMix = furLayer + 0.04;
                float rimFade = invNdotV * layerMix;
                float rimPow = rimFade * rimFade;
                rimAdd = ambientLight * rimPow * _RimIntensity;

                float rimT = invNdotV - _RimStart;
                float rimRange = max(_RimEnd - _RimStart, 1e-5);
                float rimMask = FurSmoothStep01(clamp(rimT / rimRange, 0.0, 1.0));
                rimMask *= max(_RimInt, 0.0);
                rimOut.xyz = _RimColor.rgb * max(_RimInt, 0.0);
                rimOut.w = layerMix * rimMask;
            }

            out V2F v2f;
            void main()
            {
                float furLayer = EffectiveFurLayerScale();

                float normalLen;
                vec3 growNormal = ComputeWindBlendedNormal(aNormal, normalLen);

                float baseWeight = 0.0;

                vec2 baseUV = aTexcoord0 * vec2(1,-1);
                vec4 furDir = textureLod(_FurDirectionMap, baseUV, 0.0);

                float lenScale = clamp(_FurLengthScale * _FurLength, 0.0, 1.0);
                lenScale = mix(lenScale, _FurLength, _AdaptiveScale);
                float extrude = (1.0 - baseWeight) * lenScale * furLayer * furDir.z;

                float mainAlphaInv = 1.0 - furDir.z;
                float edgeSmoothX = clamp(mainAlphaInv * (-_LayerEdgeSmoothnessStrength) + 1.2, 0.0, 1.0);
                float edgeSmoothY = mainAlphaInv * (-_LayerEdgeSmoothnessStrength) + 1.0;
                extrude *= edgeSmoothX;

                vec3 localPos = aNormal * baseWeight * _FurLength + aPosition;
                localPos += growNormal * extrude;

                float gravityScale = exp2(log2(max(furLayer, 1e-5)) * 1.5);
                vec3 gravityRaw = clamp(gravityScale * _GForce, vec3(-2.0), vec3(2.0));
                vec3 gravityBlended = mix(gravityRaw, gravityRaw * normalLen - gravityRaw, _AdaptiveScale);
                localPos += gravityBlended * extrude;

                vec3 worldPos = ObjectToWorldPos(localPos);
                gl_Position = WorldToClipPos(worldPos);
 

                vec3 worldNormal = ObjectToWorldN(aNormal);
                vec3 viewDir = normalize(_CameraPosition - worldPos);

                vec2 furUVOffset = furLayer * _FurOffset.xy;
                vec2 furDirXY = furDir.xy * 2.0 - 1.0;
                vec2 furUV = baseUV + furDirXY * furUVOffset;

                vec3 ambientLight = vec3(0.0);
                vec3 directLight = ComputeFurMainLight(worldNormal, 1.0, furLayer);
                if(_EnablePbrLight)
                {
                    Surface s;
                    s.basecolor = vec3(1.0);
                    s.metallic = 0.0;
                    s.roughness = 0.1;
                    s.normal = worldNormal;
                    s.ao = mix(0.0,1.0,furLayer);
                    ambientLight = CaclIBL(s,viewDir);
                }
                else
                {
                    ambientLight = vec3( mix(0.0,1.0,furLayer));
                }

                vec3 rimAdd;
                vec4 rimOut;
                ComputeFurRim(worldNormal, viewDir, ambientLight, furLayer, rimAdd, rimOut);
                directLight += rimAdd;

                v2f.uv = vec4(furUV, baseUV);
                v2f.worldNormal = worldNormal;
                v2f.viewDir = viewDir;
                v2f.worldPos = worldPos;
                v2f.furDirection = furDir;

                v2f.vertexLighting = ambientLight + directLight + rimAdd;


                v2f.rim = rimOut;
                v2f.layerEdgeSmooth = edgeSmoothY;
            }
            #endif

            #ifdef FRAGMENT

            vec3 EvalAdditionalFurLights(vec3 worldPos, vec3 worldNormal, float furDirZ, float furLayer)
            {
                vec3 acc = vec3(0.0);
                float stepBlend = clamp(furLayer * _FurStepStrength + 0.7, 0.0, 1.0);

                Surface s;
                s.position = worldPos;
                s.normal = worldNormal;
                s.basecolor = vec3(1.0);
                s.roughness = 0.5;

                int count = GetLocalLightCount();
                for (int i = 0; i < MAXLOCALLIGHT; ++i)
                {
                    if (i >= count)
                        break;
                    Light L = GetLocalLight(i, s);
                    float ndotl = dot(worldNormal, L.direction);
                    float stepBase = clamp(ndotl, 0.0, 1.0);
                    float layerStep = clamp(furLayer * _FurStepStrength + ndotl + 0.2, 0.0, 1.0);
                    layerStep = layerStep * stepBlend;
                    float furLit = mix(stepBase, layerStep, furDirZ);
                    furLit *= furDirZ;
                    acc += L.color * L.attenuation * furLit;
                }
                return acc;
            }

            float EvalFurAlpha(vec2 furUV, vec3 worldNormal, vec3 viewDir, float mainAlpha, float furLayer)
            {
                float thinness = clamp(_ThinnessScale * _Thinness, 0.1, 100.0);
                thinness = mix(thinness, _Thinness, _AdaptiveScale);
                float density = texture(_furMap, furUV * thinness).r;

                float layerThreshold = furLayer * furLayer + furLayer * 0.5;
                float furMask = clamp(density * 2.0 - layerThreshold + _FurClip, 0.0, 1.0);

                if (furMask * mainAlpha - 0.2 < 0.0)
                    discard;


                float ndotV = dot(worldNormal, viewDir);
                float thickness = 1.0 - exp2(log2(max(1.0 - abs(ndotV), 1e-5)));
                furMask += thickness;

                return clamp(furMask * (1-furLayer) * mainAlpha * _FurAlpha, 0.0, 1.0);
            }

            in V2F v2f;
            out vec4 FragColor;
            void main()
            {
                float furLayer = EffectiveFurLayerScale();
                vec2 furUV = v2f.uv.xy;
                vec4 dir = v2f.furDirection;
                vec3 albedo = texture(_baseColor, furUV).rgb;


                vec3 color = v2f.vertexLighting;
                color *= albedo;

                float alpha = EvalFurAlpha(furUV, v2f.worldNormal, v2f.viewDir, 1.0, furLayer);
                alpha *= v2f.layerEdgeSmooth;

                vec3 rimDelta = v2f.rim.rgb * color - color;
                color = v2f.rim.aaa * rimDelta + color;

                FragColor = vec4(color, alpha);
            }
            #endif
            ENDGLSL
        }
    }
}
