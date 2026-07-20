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

            void main() 
            {
                FragColor = vec4(1.0, 0.0, 1.0, 1.0);
            }
            #endif
            ENDGLSL
        }
    }
}
