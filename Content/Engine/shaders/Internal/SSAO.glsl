GLSLShader
{
    Properties
    {
        
    }
    SubShader
    {
        Pass
        { 
            zwrite off
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
            uniform sampler2D _NormalXYTex;
            uniform sampler2D _DepthTex;
            uniform sampler2D _FlagTex;
            uniform sampler2D _texNoise;

            
            uniform vec3 _samples[64];

            const vec2 noiseScale = vec2(_ScreenWidth/4.0, _ScreenHeight/4.0); // screen = 800x600

            out float FragColor;
            void main() 
            {
                float depth = texture(_DepthTex, uv).r;
                if (depth >= 1.0)
                {
                    FragColor = 1.0;
                    return;
                }

                vec4 NormalXY = texture(_NormalXYTex, uv);
                int shadingModel = int(texture(_FlagTex, uv).x * 255.0 + 0.5);
                vec3 NormalWS = (shadingModel == 2)
                    ? DecodeNormalEndfield(NormalXY.xy)
                    : DecodeNormalOct(NormalXY.xy);

                vec3 posVS = ReconstructPositionVS(uv, depth);
                vec3 normalVS = normalize(mat3(GL_MATRIX_V) * NormalWS);

                vec3 randomVec = texture(_texNoise, noiseScale * uv).xyz;

                vec3 tangent = normalize(randomVec - normalVS * dot(randomVec, normalVS));
                vec3 bitangent = cross(normalVS, tangent);
                mat3 tbn = mat3(tangent, bitangent, normalVS);

                float occlusion = 0.0;
                for (int i = 0; i < 64; i++)
                {
                    vec3 samplePos = tbn * _samples[i];
                    samplePos = posVS + samplePos * _ssao_raduis;

                    vec4 offset = GL_MATRIX_P * vec4(samplePos, 1.0);
                    offset.xyz /= offset.w;
                    offset.xyz = offset.xyz * 0.5 + 0.5;

                    float sampleDepth = texture(_DepthTex, offset.xy).r;
                    if (sampleDepth >= 1.0)
                        continue;

                    float sampleViewZ = ReconstructPositionVS(offset.xy, sampleDepth).z;

                    float rangeCheck = smoothstep(0.0, 1.0, _ssao_raduis / abs(posVS.z - sampleViewZ));
                    occlusion += (sampleViewZ >= samplePos.z + _ssao_bias ? 1.0 : 0.0) * rangeCheck;
                }

                occlusion = 1.0 - (occlusion / 64) * _ssao_intensity;
                FragColor = pow(occlusion,_ssao_pow);
            }
            #endif
            ENDGLSL
        }
    }
}