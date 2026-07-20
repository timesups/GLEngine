GLSLShader
{
    Properties
    {

    }
    SubShader
    {
        Pass
        { 
            cull front
            GLSLPROGRAM
            #include "include/Core.glsl"


            #ifdef VERTEX
            out vec3 normal;
            void main()
            {
                gl_Position = ObjectToClipPos(aPosition);
                normal = ObjectToWorldN(aNormal);
            }
            #endif


            #ifdef FRAGMENT

            in vec3 normal;


            out vec4 FragColor;

            void main() 
            {
                //vectors
                vec3 NormalWS = normalize(normal);
                FragColor = vec4(EncodeNormalOct(NormalWS),gl_FragCoord.z,0);
            }
            #endif
            ENDGLSL
        }
    }
}