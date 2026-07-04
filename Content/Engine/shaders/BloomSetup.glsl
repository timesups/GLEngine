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
                float threshold = _bloom_threadshold;
                float softKnee = _bloom_softkeen;

                vec3 color = texture(_SceneColor, uv).rgb;
                color = max(color,vec3(0.0));
                float lum = dot(color, vec3(0.2126, 0.7152, 0.0722));

                float rq = clamp(lum - threshold + softKnee, 0.0, 2.0 * softKnee);
                rq = (rq * rq) / (4.0 * softKnee + 1e-6);
                float contrib = max(rq, lum - threshold) / max(lum, 1e-6);

                FragColor = vec4(color * contrib, 1.0);
            }
            #endif
            ENDGLSL
        }
    }
}