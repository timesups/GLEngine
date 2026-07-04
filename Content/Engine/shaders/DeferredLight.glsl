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
            #include "Core.glsl"
            #include "Lighting.glsl"

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
            layout (binding=0) uniform sampler2D _AlbdeoAOTex;
            layout (binding=1) uniform sampler2D _NormalXYTex;
            layout (binding=2) uniform sampler2D _MRSCTex;
            layout (binding=3) uniform sampler2D _FlagTex;
            layout (binding=4) uniform sampler2D _DepthTex;
            layout (binding=5) uniform sampler2D _SSAO;

            vec3 ComputeFurMainLight(vec3 worldNormal, float furDirZ, float furLayer)
            {
                vec3 lightDir = normalize(-_MainLightDirection);
                float ndotl = dot(worldNormal, lightDir);
                float stepBase = clamp(ndotl, 0.0, 1.0);
                vec3 lightColor = _MainLightColor;

                vec3 term0 = (1.0 - furDirZ) * stepBase * lightColor;

                float layerStep = clamp(furLayer * 0.5 + ndotl + 0.2, 0.0, 1.0);
                vec3 term1 = layerStep * lightColor;
                float layerStep2 = clamp(furLayer * 0.5 + 0.7, 0.0, 1.0);
                term1 *= layerStep2;
                term1 *= furDirZ;
                term0 = term1 + term0;
                return term0;
            }

            out vec4 FragColor;

            void main() 
            {
                float depth = texture(_DepthTex, uv).r;
                if (depth >= 1.0)
                    discard;

                vec4 AlbdeoAO = texture(_AlbdeoAOTex,uv);
                vec4 NormalXY= texture(_NormalXYTex,uv);
                vec4 MRSC = texture(_MRSCTex,uv);
                vec4 Flag = texture(_FlagTex,uv);


                float ssao = texture(_SSAO,uv).x; 
                int shadingModel = int(Flag.x * 255.0) ;

                FragColor = vec4(shadingModel);
                vec3 NormalWS = (shadingModel == 2)
                    ? DecodeNormalEndfield(NormalXY.xy)
                    : DecodeNormalOct(NormalXY.xy);


                if(shadingModel == 0)
                {
                    FragColor = vec4(vec3(AlbdeoAO.rgb),1.0);
                }
                else if(shadingModel == 1)
                {
                    vec3 positionWS = ReconstructPositionWS(uv, depth);

                    Surface s;
                    s.basecolor = AlbdeoAO.rgb;
                    s.normal = NormalWS;
                    s.position = positionWS;
                    s.metallic = MRSC.x;
                    s.roughness = MRSC.y;
                    s.ao = AlbdeoAO.a * ssao;
                    s.posMainLight = _MainLightMatrix * vec4(s.position, 1.0);
                    vec3 CameraVecWS = normalize(_CameraPosition - s.position);
                    vec3 color = CalcAllLight(s,CameraVecWS);

                    
                    FragColor = vec4(vec3(color), 1.0);
                }
                else if(shadingModel == 2)
                {

                    FragColor = vec4(NormalXY.xyz, 1.0);
                }
                else if(shadingModel == 3)
                {
                    vec3 basecolor = AlbdeoAO.rgb;
                    vec3 finalColor =  basecolor * ComputeFurMainLight(NormalWS,0.5,0.5);
                    vec3 ambientColor = _AmbientColor * basecolor ;
                    finalColor += ambientColor;
                    FragColor = vec4(finalColor *0.9,1.0);
                }
                else
                {
                    FragColor = AlbdeoAO;
                }
                //输出Gbuffer的深度
                gl_FragDepth = depth;
      
            }
            #endif
            ENDGLSL
        }
    }
}