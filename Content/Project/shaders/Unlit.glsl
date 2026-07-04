GLSLShader
{
    Properties
    {
        sampler2D _baseColor
    }
    SubShader
    {
        Pass
        { 
            cull back
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

            
            out vec4 FragColor;
            in vec2 uv;
            uniform sampler2D _baseColor;
            void main() 
            {
                vec4 color = texture(_baseColor,uv);
                FragColor = vec4(_PassIndex);
            }
            #endif
            ENDGLSL
        }
    }
}