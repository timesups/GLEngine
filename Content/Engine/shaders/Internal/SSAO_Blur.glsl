GLSLShader
{
    Properties
    {
        
    }
    SubShader
    {
        Pass
        { 
            zwrite off
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
            out float FragColor;
            void main() 
            {
                vec2 texelSize = 1.0 / vec2(textureSize(_SceneColor, 0));
                float result = 0.0;
                for (int x = -2; x < 2; ++x) 
                {
                    for (int y = -2; y < 2; ++y) 
                    {
                        vec2 offset = vec2(float(x), float(y)) * texelSize;
                        result += texture(_SceneColor, uv + offset).r;
                    }
                }
                FragColor = result / (4.0 * 4.0);
            }
            #endif
            ENDGLSL
        }
    }
}