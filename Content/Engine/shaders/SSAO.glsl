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
            uniform sampler2D _texNoise;

            
            uniform vec3 _samples[64];

            const vec2 noiseScale = vec2(_ScreenWidth/4.0, _ScreenHeight/4.0); // screen = 800x600

            out float FragColor;
            void main() 
            {
                //重建世界位置和世界法线
                float depth = texture(_DepthTex, uv).r;
                vec4 NormalXY= texture(_NormalXYTex,uv);
                vec3 NormalWS = DecodeNormalOct(NormalXY.xy);
                vec3 positionWS = ReconstructPositionWS(uv, depth);

                //转换成视口空间法线
                vec3 posVS = (GL_MATRIX_V * vec4(positionWS,1.0)).xyz;
                vec3 normalVS = normalize(transpose(inverse(mat3(GL_MATRIX_V))) * NormalWS);


                
                vec3 randomVec = texture(_texNoise,noiseScale * uv).xyz;

                vec3 tangent = normalize(randomVec - normalVS * dot(randomVec,normalVS));
                vec3 bitangent = cross(normalVS,tangent);
                mat3 tbn = mat3(tangent,bitangent,normalVS);

                float occlusion = 0.0;
                for(int i = 0;i<64;i++)
                {
                    vec3 samplePos = tbn * _samples[i];
                    samplePos = posVS + samplePos * _ssao_raduis ;

                    vec4 offset = GL_MATRIX_P * vec4(samplePos,1.0);
                    offset.xyz  /= offset.w;
                    offset.xyz = offset.xyz * 0.5 + 0.5;

                    float sampleDepth = -LinearizeDepth(texture(_DepthTex, offset.xy).r); 


                    float rangeCheck = smoothstep(0.0, 1.0, _ssao_raduis / abs(posVS.z - sampleDepth ));

                    occlusion  += (sampleDepth >= samplePos.z + _ssao_bias ? 1.0 : 0.0) * rangeCheck ;    
                }

                occlusion = 1.0 - (occlusion / 64) * _ssao_intensity;
                FragColor = pow(occlusion,_ssao_pow);
            }
            #endif
            ENDGLSL
        }
    }
}