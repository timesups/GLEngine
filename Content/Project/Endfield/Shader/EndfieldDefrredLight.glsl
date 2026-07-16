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

            layout (binding=0) uniform sampler2D outGBuffer0;//final color
            layout (binding=1) uniform sampler2D outGBuffer1;// velocity
            layout (binding=2) uniform sampler2D outGBuffer2;// attr
            layout (binding=3) uniform sampler2D outGBuffer3;//normalEnc
            layout (binding=4) uniform sampler2D outGBuffer4;//albedo
            layout (binding=5) uniform sampler2D outGBuffer5;//depth
            layout (binding=6) uniform sampler2D shadowMap;//depth
            out vec4 FragColor;


            void main() 
            {
                float depth = texture(outGBuffer5,uv).r;
                if (depth >= 1.0)
                    discard;
                vec4 hdrColor = texture(outGBuffer0, uv);
                float mainLightShadow = texture(shadowMap,uv).x;

                vec2 normalEnc = texture(outGBuffer3,uv).xy;
                vec3 albedo = texture(outGBuffer4,uv).rgb;
                vec3 positionWS = ReconstructPositionWS(uv,depth);
                vec3 normalWS = DecodeNormalEndfield(normalEnc);

                


                FragColor = vec4(vec3(albedo),1.0);
            }
            #endif
            ENDGLSL
        }
    }
}