GLSLShader
{
    Properties
    {
        
    }
    SubShader
    {
        Pass
        { 
            name fur
            zwrite off
            cull off
            blend srcalpha oneminussrcalpha
            GLSLPROGRAM
            #include "Core.glsl"
            #include "Light.glsl"
            #ifdef VERTEX
            out vec2 uv0;
            out vec2 uv1;
            void main()
            {
                float forwardOffset = 0.0;
                float normalScale = -8.0;
                float planeScale =  50.0;
                float offsetBlend = 0.5;

                vec3 forward = normalize(GL_I_MATRIX_V * vec4(0,0,1,0)).xyz;
                vec3 right = normalize(cross(vec3(0,1,0),forward));
                vec3 up = normalize(cross(forward,right));


                vec3 newPos = ObjectToWorldPos(aPosition);

                newPos += forward * forwardOffset;

                vec3 normalOffset = normalize(aNormal * 2.0-1.0) * normalScale;
                vec3 planeOffset = right * (aTexcoord1.x-0.5) * planeScale + up * (aTexcoord1.y-0.5) * planeScale;

                newPos += mix(normalOffset,planeOffset,offsetBlend);


                gl_Position = WorldToClipPos(newPos);

                uv0 = aTexcoord0;
                uv1 = aTexcoord1;

            }
            #endif


            #ifdef FRAGMENT
            in vec2 uv0;
            in vec2 uv1;
            uniform sampler2D _colorTex;
            uniform sampler2D _furTex;
            out vec4 FragColor;
            void main() 
            {
                vec4 texColor = texture(_colorTex,uv0);
                vec4 fur = texture(_furTex,uv1);

                FragColor = vec4(texColor.xyz ,fur.a);
            }
            #endif
            ENDGLSL
        }
        Pass
        { 
            cull back
            GLSLPROGRAM
            #include "Core.glsl"
            #include "Light.glsl"
            #ifdef VERTEX
            out vec2 uv0;
            out vec2 uv1;
            void main()
            {
                gl_Position = ObjectToClipPos(aPosition);
                uv0 = aTexcoord0;
                uv1 = aTexcoord1;

            }
            #endif

            #ifdef FRAGMENT
            in vec2 uv0;
            in vec2 uv1;
            uniform sampler2D _colorTex;
            uniform sampler2D _furTex;
            out vec4 FragColor;
            void main() 
            {
                vec4 texColor = texture(_colorTex,uv0);
                FragColor = vec4(texColor.xyz ,1.0);
            }
            #endif
            ENDGLSL
        }

    }
}