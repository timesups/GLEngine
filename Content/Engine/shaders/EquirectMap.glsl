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
            #include "Core.glsl"
            #ifdef VERTEX
            out vec3 locPos;
            uniform mat4 _GL_MATRIX_P;
            uniform mat4 _GL_MATRIX_V;
            void main()
            {
                locPos = aPosition;
                gl_Position = _GL_MATRIX_P * _GL_MATRIX_V * vec4(aPosition,1.0);
            }
            #endif

            #ifdef FRAGMENT
            out vec4 FragColor;
            in vec3 locPos;
            uniform sampler2D _EquirectMap;


            vec2 SampleSphericalMap(vec3 v)
            {
                vec2 uv = vec2(atan(v.z, v.x), asin(clamp(v.y, -1.0, 1.0)));
                uv *= vec2(0.1591, 0.3183);
                uv += 0.5;
                return uv;
            }

            void main() 
            {
                vec2 uv = SampleSphericalMap(normalize(locPos));
                
                vec3 texColor = texture(_EquirectMap,uv).xyz;
                FragColor = vec4(texColor,1.0);
            }
            #endif
            ENDGLSL
        }
    }
}