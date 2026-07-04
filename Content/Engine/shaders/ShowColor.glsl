GLSLShader
{
    Properties
    {
        
    }
    SubShader
    {
        Pass
        { 
            GLSLPROGRAM

            #ifdef VERTEX
            #include "/include/Core.glsl"
            
            void main()
            {
                gl_Position = ObjectToClipPos(aPosition);
            }
            #endif

            #ifdef FRAGMENT
            out vec4 FragColor;
            uniform vec3 _Color;
            void main() 
            {
                FragColor = vec4(_Color,1.0);
            }
            #endif
            ENDGLSL
        }
    }
}