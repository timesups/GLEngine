GLSLShader
{
    Properties
    {
        float _L = 1.0
        float _A = 2.0
        float _S = 1.0
        vec2 _D = vec2(1.0,0.0)
    }
    SubShader
    {
        Pass
        { 
            cull off
            GLSLPROGRAM
            #include "Core.glsl"
            #include "Lighting.glsl"
            uniform float _L ;
            uniform float _A;
            uniform float _S;
            uniform vec2 _D;

            struct V2F
            {
                vec3 NormalWS;
                vec3 TangentWS;
                vec3 BNormalWS;
                vec3 PositionWS;
                vec3 CamVectorWS;
                vec2 uv;
                vec4 posMainLight;

            };
            #ifdef VERTEX


            out V2F v2f;
            void main()
            {
                




                v2f.NormalWS = ObjectToWorldN(aNormal);

                v2f.TangentWS = ObjectToWorldN(aTangent.xyz);


                v2f.TangentWS = normalize(v2f.TangentWS-dot(v2f.TangentWS,v2f.NormalWS) * v2f.NormalWS);
                v2f.BNormalWS = cross(v2f.NormalWS,v2f.TangentWS) * aTangent.w;
                v2f.PositionWS = ObjectToWorldPos(aPosition);
                v2f.CamVectorWS = _CameraPosition - v2f.PositionWS;
                v2f.uv = aTexcoord0;
                v2f.posMainLight = _MainLightMatrix * GetModelMatrix() * vec4(aPosition,1.0);

                vec3 posWS = ObjectToWorldPos(aPosition);

                float o = 2 * PI / _L;
                float f = _S * o;

                float height = _A * sin(dot(normalize(_D) , vec2(posWS.x,posWS.z) )* o + _time * f);



                gl_Position = WorldToClipPos(vec3(posWS.x,height,posWS.z));


            }
            #endif

            #ifdef FRAGMENT

            in V2F v2f;
            out vec4 FragColor;
            void main() 
            {
                vec3 NormalWS = normalize(v2f.NormalWS);
                vec3 PositionWS = v2f.PositionWS;


                Surface s;
                s.basecolor = vec3(1.0);
                s.normal = NormalWS;
                s.position = PositionWS;
                s.roughness = 0.5;
                s.posMainLight = v2f.posMainLight;
                s.ao = 1.0;

                s.ao = 1.0;

                s.position = PositionWS;
                s.posMainLight = v2f.posMainLight;
                vec3 CameraVecWS = normalize(v2f.CamVectorWS);
                vec3 color = CalcAllLight(s,CameraVecWS);
                
                FragColor = vec4(color,1.0);
            }
            #endif
            ENDGLSL
        }
    }
}