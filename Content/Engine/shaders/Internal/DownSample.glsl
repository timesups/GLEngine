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
            uniform vec4 _InvSrcSize; //jsut x,y
            out vec4 FragColor;
            void main() 
            {
                vec2 dstCenter = gl_FragCoord.xy;
                vec2 scrCenter = (dstCenter + 0.5) * 2.0 - 0.5;

                vec2 t = _InvSrcSize.xy;
                vec2 uv00 = (scrCenter + vec2(-0.5,-0.5)) * t;
                vec2 uv10 = (scrCenter + vec2(0.5,-0.5)) * t;
                vec2 uv01 = (scrCenter + vec2(-0.5,0.5)) * t;
                vec2 uv11 = (scrCenter + vec2(0.5,0.5)) * t;

                vec4 sum = 
                        texture(_SceneColor,uv00) + 
                        texture(_SceneColor,uv10) + 
                        texture(_SceneColor,uv01) + 
                        texture(_SceneColor,uv11);

                FragColor = vec4(max(sum.rgb * 0.25,vec3(0.0)),sum.a * 0.25);
            }
            #endif
            ENDGLSL
        }
    }
}