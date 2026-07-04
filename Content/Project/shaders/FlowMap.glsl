GLSLShader
{
    Properties
    {
        sampler2D _MainTex
        sampler2D _FlowMap

        float _velocity = 0.1
        float _timeSpeed = 0.1

    }
    SubShader
    {
        Pass
        { 
            GLSLPROGRAM
            #include "Core.glsl"
            #ifdef VERTEX
            out vec2 uv;
            void main()
            {
                gl_Position = ObjectToClipPos(aPosition);
                uv = aTexcoord0;
 
            }
            #endif
            #ifdef FRAGMENT
            uniform sampler2D _MainTex;
            uniform sampler2D _FlowMap;
            uniform float _velocity;
            uniform float _timeSpeed;
            in vec2 uv;
            out vec4 FragColor;
            void main() 
            {   
                vec4 flow = texture(_FlowMap,uv) *2-1;
                flow *= -_velocity;

                float phase0 = fract(_time * 0.1 * _timeSpeed+0.5);
                float phase1 = fract(_time * 0.1 * _timeSpeed+1.0);



                vec3 color0 = texture(_MainTex,uv+(flow.xy*phase0)).xyz;
                vec3 color1 = texture(_MainTex,uv+(flow.xy*phase1)).xyz;

                float flowLerp = abs((0.5-phase0)/0.5);

                vec3 finalColor = mix(color0,color1,flowLerp);


                FragColor =vec4(vec3(finalColor.xyz),1.0);
            }
            #endif
            ENDGLSL
        }
    }
}