GLSLShader
{
    Properties
    {
    }
    SubShader
    {
        Pass
        {
            cull off
            zwrite off
            ztest always
            GLSLPROGRAM
            #include "/include/Core.glsl"

            #ifdef VERTEX
            out vec2 uv;

            void main()
            {
                uv = aPosition.xy * 0.5 + 0.5;
                vec3 worldCenter = (GetModelMatrix() * vec4(0.0, 0.0, 0.0, 1.0)).xyz;
                vec3 posWS = (GetModelMatrix() * vec4(aPosition,1.0)).xyz;

                vec2 newUV = vec2(uv.x,1-uv.y);
                newUV -= vec2(0.5,0.5);
                newUV /= max(sqrt(dot(newUV,newUV)),0.00001);
                newUV *= distance(posWS,worldCenter) * 0.2;
                vec3 newPos = vec3(newUV.x,newUV.y,0.0);

                newPos = inverse(mat3(GL_MATRIX_V)) * newPos;
                newPos += worldCenter;


                gl_Position = GL_MATRIX_P  * GL_MATRIX_V *vec4(newPos,1.0);
            }
            #endif
            #ifdef FRAGMENT
            in vec2 uv;
            out vec4 FragColor;

            uniform sampler2D _iconMap;

            void main()
            {
                vec4 texColor =  texture(_iconMap, uv);
                if(texColor.a <=0.0)
                    discard;
                FragColor = texColor;
            }
            #endif
            ENDGLSL
        }
    }
}
