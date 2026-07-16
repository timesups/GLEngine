GLSLShader
{
    Properties
    {
        
    }
    SubShader
    {
        Pass
        { 
            Cull off
            GLSLPROGRAM
            #include "/include/Core.glsl"
            #include "/include/Surface.glsl"
            #include "/include/Light.glsl"
            #include "/include/Functions.glsl"

            #ifdef VERTEX
            out vec2 uv;
            void main()
            {
                gl_Position = vec4(aPosition.xy,0.0,1.0);
                uv = vec2(aPosition.xy * 0.5+0.5);
            }
            #endif

            #ifdef FRAGMENT

            layout (binding=0) uniform sampler2D outGBuffer0;//final color
            layout (binding=1) uniform sampler2D outGBuffer1;// velocity
            layout (binding=2) uniform sampler2D outGBuffer2;// attr
            layout (binding=3) uniform sampler2D outGBuffer3;//normalEnc
            layout (binding=4) uniform sampler2D outGBuffer4;//albedo
            layout (binding=5) uniform sampler2D outGBuffer5;//depth

            layout (binding=6) uniform sampler2D _MainLightDepth;//final color
            layout (binding=7) uniform sampler2D _LocalLightDepth;//final color

            in vec2 uv;

            out vec4 FragColor;

            // 光空间 receiver 平面 dz/duv
            vec2 ComputeReceiverPlaneDepthBias(vec3 shadowCoord)
            {
                vec3 duvdx = dFdx(shadowCoord);
                vec3 duvdy = dFdy(shadowCoord);
                float det = duvdx.x * duvdy.y - duvdx.y * duvdy.x;
                vec2 dz_duv;
                dz_duv.x =  duvdy.y * duvdx.z - duvdx.y * duvdy.z;
                dz_duv.y = -duvdy.x * duvdx.z + duvdx.x * duvdy.z;
                return dz_duv / (det + 1e-7);
            }

            float SampleShadow(vec2 uv, float receiverDepth)
            {
                float closest = texture(_MainLightDepth, uv).x;
                return receiverDepth > closest ? 1.0 : 0.0;
            }

            // Soft PCF：The Witness 风格 4-tap 加权核（近似 5x5），每 tap 带 Receiver Plane Bias
            float SoftPCF(vec3 shadowCoord, vec2 dz_duv, float bias, vec2 texelSize)
            {
                vec2 shadowMapSize = 1.0 / texelSize;
                vec2 uv = shadowCoord.xy * shadowMapSize;
                vec2 base = floor(uv + 0.5);
                vec2 s = uv + 0.5 - base;

                base = (base - 0.5) * texelSize;

                vec2 uw = vec2(3.0 - 2.0 * s.x, 1.0 + 2.0 * s.x);
                vec2 vw = vec2(3.0 - 2.0 * s.y, 1.0 + 2.0 * s.y);
                vec2 u = vec2((2.0 - s.x) / uw.x - 1.0, s.x / uw.y + 1.0);
                vec2 v = vec2((2.0 - s.y) / vw.x - 1.0, s.y / vw.y + 1.0);

                float zw = shadowCoord.z - bias;
                vec2 o00 = vec2(u.x, v.x) * texelSize;
                vec2 o10 = vec2(u.y, v.x) * texelSize;
                vec2 o01 = vec2(u.x, v.y) * texelSize;
                vec2 o11 = vec2(u.y, v.y) * texelSize;

                float shadow = 0.0;
                shadow += uw.x * vw.x * SampleShadow(base + o00, zw + dot(dz_duv, o00));
                shadow += uw.y * vw.x * SampleShadow(base + o10, zw + dot(dz_duv, o10));
                shadow += uw.x * vw.y * SampleShadow(base + o01, zw + dot(dz_duv, o01));
                shadow += uw.y * vw.y * SampleShadow(base + o11, zw + dot(dz_duv, o11));
                return shadow * (1.0 / 16.0);
            }

            float CalcShadow(Light light, vec3 positionWS)
            {
                vec4 positionsLS = _MainLightMatrix * vec4(positionWS, 1.0);
                vec3 shadowCoord = positionsLS.xyz / positionsLS.w;
                shadowCoord = shadowCoord * 0.5 + 0.5;

                // 导数必须在分支前计算，避免邻像素发散导致错误
                vec2 dz_duv = ComputeReceiverPlaneDepthBias(shadowCoord);
                vec2 texelSize = 1.0 / vec2(textureSize(_MainLightDepth, 0));

                if (shadowCoord.z > 1.0)
                    return 0.0;

                return SoftPCF(shadowCoord, dz_duv, light.bias, texelSize);
            }

            void main() 
            {   
                float depth = texture(outGBuffer5, uv).r;
                if (depth >= 1.0)
                {
                    FragColor = vec4(1.0);
                    return;
                }

                vec2 normalEnc = texture(outGBuffer3, uv).xy;
                vec3 positionWS = ReconstructPositionWS(uv, depth);
                vec3 normalWS = DecodeNormalEndfield(normalEnc);

                Surface s;
                s.normal = normalWS;
                s.position = positionWS;

                Light mainLight = GetMainLight(s);

                float shadowMap = 1.0 - CalcShadow(mainLight, positionWS);
       
                FragColor = vec4(vec3(shadowMap), 1.0);
            }
            #endif
            ENDGLSL
        }
    }
}