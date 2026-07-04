GLSLShader
{
    Properties
    {
        vec4 _Color
    }
    SubShader
    {
        Pass
        { 
            queue Transparent
            cull front
            GLSLPROGRAM
            #include "Core.glsl"

            #ifdef VERTEX
            void main()
            {
                gl_Position = ObjectToClipPos(aPosition);
 
            }
            #endif
            #ifdef FRAGMENT
            uniform vec4 _Color;
            out vec4 FragColor;
            void main() 
            {
                FragColor = _Color * 10.0;
            }
            #endif
            ENDGLSL
        }
    }
}