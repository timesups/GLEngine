GLSLShader
{
    Properties
    {
        
    }
    SubShader
    {
        Pass
        { 
            Cull off
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

            layout (binding=0) uniform sampler2D outGBuffer0;
            layout (binding=1) uniform sampler2D outGBuffer1;
            layout (binding=2) uniform sampler2D outGBuffer2;
            layout (binding=3) uniform sampler2D outGBuffer3;
            layout (binding=4) uniform sampler2D outGBuffer4;
            out vec4 FragColor;
            void main() 
            {
                vec3 hdrColor = texture(outGBuffer4, uv).rgb;
                FragColor = vec4(hdrColor,1.0);
            }
            #endif
            ENDGLSL
        }
    }
}