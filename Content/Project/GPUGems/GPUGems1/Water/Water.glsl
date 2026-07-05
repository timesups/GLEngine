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
            GLSLPROGRAM
            #include "Core.glsl"
            struct V2F
            {
                vec3 worldpos;
                vec3 worldnor;
            };
            #ifdef VERTEX

            void SinWave(float A,float L,float S,vec2 D,inout vec3 pos,inout vec3 n)
            {
                D = normalize(D);
                float omega = 2 * PI / L;
                float phi = S * omega;
                float hight = A * sin(dot(D,pos.xz) * omega + _time*phi);

                pos = vec3(pos.x,pos.y+hight,pos.z);

                float nx = -1 * omega * D.x * A * cos(dot(D,pos.xz) * omega + _time* phi);
                float nz = -1 * omega * D.y * A * cos(dot(D,pos.xz) * omega + _time* phi);
                n = vec3(nx,1,nz);
                n = normalize(n);
            }

            out V2F v2f;
            void main()
            {
                vec3 localPos = aPosition;
                vec3 normal = vec3(0.0);
                SinWave(0.5,50,10,vec2(1,0),localPos,normal);

                vec3 worldpos = ObjectToWorldPos(localPos);

                v2f.worldpos = worldpos;
                v2f.worldnor = normal;

                gl_Position = WorldToClipPos(worldpos);
            }
            #endif

            #ifdef FRAGMENT

            in V2F v2f;
            out vec4 FragColor;
            void main() 
            {
                FragColor = vec4(v2f.worldnor,1);
            }
            #endif
            ENDGLSL
        }
    }
}