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
            #include "Lighting.glsl"
            struct V2F
            {
                vec3 worldpos;
                vec3 worldnor;
            };
            #ifdef VERTEX

            void SinWave(float A,float L,float S,float K,vec2 D,inout vec3 pos,inout vec3 n)
            {
                vec2 xzplane = pos.xz;
                D = normalize(D);
                float omega = 2 * PI / L;
                float phi = S * omega;
                float hight = 2 * A * pow((sin(dot(D,xzplane) * omega + _time*phi)+1)/2,K);

                pos = vec3(pos.x,pos.y+hight,pos.z);

                float nx = -1 * K * omega * D.x * A * pow((sin(dot(D,xzplane) * omega + _time*phi)+1)/2,K-1) *cos(dot(D,xzplane) * omega + _time * phi);
                float nz = -1 * K * omega * D.y * A * pow((sin(dot(D,xzplane) * omega + _time*phi)+1)/2,K-1) *cos(dot(D,xzplane) * omega + _time * phi);
                n = vec3(n.x + nx,n.y+1,n.z+nz);
                n = normalize(n);
            }

            out V2F v2f;
            void main()
            {
                vec3 localPos = aPosition;
                vec3 normal = vec3(0.0);

                vec2 D = localPos.xz - vec2(0.2);

                SinWave( 10, 50, 50, 10,D,localPos,normal);


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
                Surface s;
                s.position = v2f.worldpos;
                s.normal = normalize(v2f.worldnor);

                Light mainLight = GetMainLight(s);


                float lambert = max(dot(mainLight.direction,s.normal),0.0);


                FragColor = vec4(lambert);
            }
            #endif
            ENDGLSL
        }
    }
}