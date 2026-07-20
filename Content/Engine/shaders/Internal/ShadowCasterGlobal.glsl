GLSLShader
{
    Properties
    {
        
    }
    SubShader
    {
        Pass
        { 
            cull off
            GLSLPROGRAM
            #include "include/Core.glsl"
            #ifdef VERTEX
            void main()
            {
                gl_Position = _MainLightMatrix * GetModelMatrix() * vec4(aPosition,1.0);
            }
            #endif

            #ifdef FRAGMENT

            void main() 
            {
                //gl_FragDepth = gl_FragCoord.z;
            }
            #endif
            ENDGLSL
        }
    }
}