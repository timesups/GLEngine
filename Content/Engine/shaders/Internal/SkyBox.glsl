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
            ztest lequal
            GLSLPROGRAM
            #ifdef VERTEX
            #include "/include/Core.glsl"
            out vec3 texcoord;
            void main()
            {
                vec4 pos = GL_MATRIX_P * GL_MATRIX_V_NO_MOVE *vec4(aPosition,1.0);
                gl_Position = pos.xyww;
                texcoord = normalize(aPosition);
            }
            #endif

            #ifdef FRAGMENT
            uniform samplerCube skybox;
            uniform float _skyBoxBrightness;
            in vec3 texcoord;
            out vec4 FragColor;
            void main() 
            {
                vec3 skyColor = textureLod(skybox,texcoord,0).xyz * _skyBoxBrightness;
                FragColor = vec4(skyColor ,1.0);
            }
            #endif
            ENDGLSL
        }
    }
}