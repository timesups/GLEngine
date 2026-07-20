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
            uniform sampler2D _SceneColor;
            out vec4 FragColor;
            void main() 
            {
                FragColor = texture(_SceneColor,uv);
            }
            #endif
            ENDGLSL
        }
    }
}