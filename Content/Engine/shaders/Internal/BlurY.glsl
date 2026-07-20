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
            uniform vec4 _InvSrcSize; // x=1/w,y=1/h,z=w,w=h
            out vec4 FragColor;


            uniform float offset[3] = float[](0.0, 1.3846, 3.2307);
            uniform float weight[3] = float[](0.2270, 0.3162, 0.0702);


            void main() 
            {
                float InvHeight = _InvSrcSize.y;
                //vertiacal blur
                vec3 hdrColor_blur = texture(_SceneColor,uv).rgb * weight[0];
                for(int i = 1;i<3;i++)
                {
                    float real_offset = offset[i] * InvHeight;


                    hdrColor_blur += texture(_SceneColor,uv + vec2(0,real_offset)).rgb * weight[i];
                    hdrColor_blur += texture(_SceneColor,uv - vec2(0,real_offset)).rgb * weight[i];
                }

                FragColor = vec4(hdrColor_blur,1.0);
            }
            #endif
            ENDGLSL
        }
    }
}