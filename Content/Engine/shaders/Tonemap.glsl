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
            uniform sampler2D _BloomColor;
            out vec4 FragColor;

            vec3 ApplyGammaExposure(vec3 hdrColor,float expo,float gamma)
            {
                vec3 sdrColor = vec3(1.0) - exp(-hdrColor * expo);
                return pow(sdrColor,vec3(1.0/gamma));
            }
            void main() 
            {
                float exposure =_exposure;
                float gamma = _gamma;
                float bloomIntensity =_bloom_intensity;

                vec3 hdrColor = texture(_SceneColor, uv).rgb;

                vec3 bloomColor = vec3(0.0);
                if (_bloom_enable > 0.0)
                    bloomColor = texture(_BloomColor, uv).rgb * bloomIntensity;

                vec3 combined = hdrColor + bloomColor;
                vec3 finalColor = ApplyGammaExposure(combined, exposure, gamma);

                FragColor = vec4(finalColor, 1.0);
            }
            #endif
            ENDGLSL
        }
    }
}