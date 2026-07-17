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
            #ifdef VERTEX
            out vec2 uv;
            void main()
            {
                gl_Position = vec4(aPosition.xy,0.0,1.0);
                uv = vec2(aPosition.xy * 0.5+0.5);
            }
            #endif

            #ifdef FRAGMENT
            in vec2 uv;

            layout (binding=0) uniform sampler2D outGBuffer0;//final color
            layout (binding=1) uniform sampler2D outGBuffer1;// velocity
            layout (binding=2) uniform sampler2D outGBuffer2;// attr
            layout (binding=3) uniform sampler2D outGBuffer3;//normalEnc
            layout (binding=4) uniform sampler2D outGBuffer4;//albedo
            layout (binding=5) uniform sampler2D outGBuffer5;//depth
            layout (binding=6) uniform sampler2D shadowMap;//depth
            out vec4 FragColor;
            float luma(vec3 c) {
                // 原式: 0.5*r + g + 0.5*b
                return c.x * 0.5 + c.y + c.z * 0.5;
            }

            vec3 contrastAdaptiveSharpen(vec2 uv, float sharpenStrength) {
                // _child22.zw -> float3(z,w,0); 偏移按原 swizzle 保持
                float ox = 1.0/_ScreenWidth;
                float oy = 1.0/_ScreenHeight;
                vec2 offA = vec2(oy, 0.0); // .zy
                vec2 offB = vec2(ox, 0.0); // .xz

                vec3 c  = texture(outGBuffer0,uv).rgb;
                vec3 n0 = texture(outGBuffer0, uv + offA).rgb; // +zy
                vec3 n1 = texture(outGBuffer0, uv - offB).rgb; // -xz
                vec3 n2 = texture(outGBuffer0, uv + offB).rgb; // +xz
                vec3 n3 = texture(outGBuffer0, uv - offA).rgb; // -zy

                float lC = luma(c);
                float l0 = luma(n0);
                float l1 = luma(n1);
                float l2 = luma(n2);
                float l3 = luma(n3);

                float lAvg = 0.25 * (l0 + l1 + l2 + l3);
                float edge = abs(lAvg - lC);
                float lMax = max(max(max(l0, lC), l1), max(l2, l3));
                float lMin = min(min(min(l0, lC), l1), min(l2, l3));
                float contrast = lMax - lMin;
                float soft = 1.0 - 0.5 * clamp(edge / contrast, 0.0, 1.0);

                // 邻域通道 min/max（不含中心）
                float minR = min(min(n0.r, n2.r), min(n1.r, n3.r));
                float minG = min(min(n0.g, n2.g), min(n1.g, n3.g));
                float minB = min(min(n0.b, n2.b), min(n1.b, n3.b));
                float maxR = max(max(n0.r, n2.r), max(n1.r, n3.r));
                float maxG = max(max(n0.g, n2.g), max(n1.g, n3.g));
                float maxB = max(max(n0.b, n2.b), max(n1.b, n3.b));

                float wR = max(-(minR * (0.25 / maxR)), (1.0 - maxR) / (4.0 * minR - 4.0));
                float wG = max(-(minG * (0.25 / maxG)), (1.0 - maxG) / (4.0 * minG - 4.0));
                float wB = max(-(minB * (0.25 / maxB)), (1.0 - maxB) / (4.0 * minB - 4.0));

                float w = max(-0.1875, min(max(wR, max(wG, wB)), 0.0));
                w *= sharpenStrength * soft;

                float inv = 1.0 / (4.0 * w + 1.0);
                return vec3(
                    (w * (n0.r + n1.r + n3.r + n2.r) + c.r) * inv,
                    (w * (n0.g + n1.g + n3.g + n2.g) + c.g) * inv,
                    (w * (n0.b + n1.b + n3.b + n2.b) + c.b) * inv
                );
            }

            vec3 linearToSrgb(vec3 x) {
                vec3 lo = x * 12.92;
                vec3 hi = 1.055 * pow(abs(x), vec3(0.4167)) - 0.055;
                return mix(hi, lo, lessThanEqual(x, vec3(0.0031)));
            }

            void main() 
            {
                float depth = texture(outGBuffer5,uv).r;
                if (depth >= 1.0)
                    discard;
                float mainLightShadow = texture(shadowMap,uv).x;

                vec2 normalEnc = texture(outGBuffer3,uv).xy;
                vec3 albedo = texture(outGBuffer4,uv).rgb;
                vec3 positionWS = ReconstructPositionWS(uv,depth);
                vec3 normalWS = DecodeNormalEndfield(normalEnc);




                vec3 color = contrastAdaptiveSharpen(uv,0.5);

                color *= _exposure;

                

                color = linearToSrgb(color);
                FragColor = vec4(vec3(color.xyz),1.0);
            }
            #endif
            ENDGLSL
        }
    }
}