GLSLShader
{
    Properties
    {
        float _A1 = 1
        float _L1 = 1
        float _S1 = 1
        float _Q1 = 1
        vec2 _D1 = (1,0)
        float _gravity = 9.8;

    }
    SubShader
    {
        Pass
        { 
            Tags
            {
                LightMode forward
            }
            cull off
            GLSLPROGRAM
            #include "Core.glsl"
            #include "Lighting.glsl"
            #include "OceanWave.glsl"

            uniform float _A1;
            uniform float _L1;
            uniform float _S1;
            uniform float _Q1;
            uniform vec2 _D1;

            float computeOmega(float wavelength) 
            {
                float k = 2.0 * PI / wavelength;
                return sqrt(_OceanGlobals.y * k);
            }
            
            struct V2F
            {
                vec3 worldpos;
                vec3 worldnor;
                vec3 CamVec;
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



            void GerstnerWave(float A,float L,float S,float Q,vec2 D,inout vec3 pos,inout vec3 n)
            {
                vec2 xzplane = pos.xz;
                D = normalize(D);
                float omega = computeOmega(L);
                float phi = S * omega;

                float newX = Q * A * D.x * cos(omega * dot(D,xzplane) + phi * _time);
                float newZ = Q * A * D.y * cos(omega * dot(D,xzplane) + phi * _time);
                float height = A * sin(omega * dot(D,xzplane) + _time * phi);

                pos = pos + vec3(newX,height,newZ);


                float wa = omega * A;
                float sv = sin(omega * dot(D,xzplane) + _time * phi);
                float cv = cos(omega * dot(D,xzplane) + _time * phi);

                float nx = -1 * D.x * wa * cv;
                float nz = -1 * D.y * wa * cv;
                float ny = 1 - Q * wa * sv;

                n += vec3(nx,ny,nz);
                n = normalize(n);

            }
     


            out V2F v2f;
            void main()
            {
                vec3 localPos = aPosition;
                vec3 normal = vec3(0.0);
                //vec2 D = localPos.xz - vec2(0.2);
                //SinWave( 10, 50, 50, 10,D,localPos,normal);


               int waveCount = int(_OceanGlobals.x);
               for(int i = 0; i < waveCount; ++i)
                {
                    OceanWave wave = _OceanWaves[i];
                    GerstnerWave(wave.params.x, wave.params.y, wave.params.z, wave.params.w, wave.direction.xy, localPos, normal);
                }



                vec3 worldpos = ObjectToWorldPos(localPos);

                v2f.worldpos = worldpos;
                v2f.worldnor = normal;
                v2f.CamVec = _CameraPosition - worldpos;

                gl_Position = WorldToClipPos(worldpos);
            }
            #endif

            #ifdef FRAGMENT

            in V2F v2f;
            out vec4 FragColor;
            void main() 
            {
                Surface s;
                s.basecolor = vec3(.1);
                s.position = v2f.worldpos;
                s.normal = normalize(v2f.worldnor);
                s.roughness = 0.1;
                s.metallic = 0.0;
                s.ao = 1.0;
                vec3 finalColor = CalcAllLight(s,normalize(v2f.CamVec));
                FragColor = vec4(finalColor,1.0);
            }
            #endif
            ENDGLSL
        }
        
    }

      Pass
        { 
            Tags
            {
                LightMode ShadowCaster
            }
            cull off
            GLSLPROGRAM
            #include "Lighting.glsl"
            #include "OceanWave.glsl"

            uniform float _A1;
            uniform float _L1;
            uniform float _S1;
            uniform float _Q1;
            uniform vec2 _D1;

            float computeOmega(float wavelength) 
            {
                float k = 2.0 * PI / wavelength;
                return sqrt(_OceanGlobals.y * k);
            }
            
            struct V2F
            {
                vec3 worldpos;
                vec3 worldnor;
                vec3 CamVec;
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



            void GerstnerWave(float A,float L,float S,float Q,vec2 D,inout vec3 pos,inout vec3 n)
            {
                vec2 xzplane = pos.xz;
                D = normalize(D);
                float omega = computeOmega(L);
                float phi = S * omega;

                float newX = Q * A * D.x * cos(omega * dot(D,xzplane) + phi * _time);
                float newZ = Q * A * D.y * cos(omega * dot(D,xzplane) + phi * _time);
                float height = A * sin(omega * dot(D,xzplane) + _time * phi);

                pos = pos + vec3(newX,height,newZ);


                float wa = omega * A;
                float sv = sin(omega * dot(D,xzplane) + _time * phi);
                float cv = cos(omega * dot(D,xzplane) + _time * phi);

                float nx = -1 * D.x * wa * cv;
                float nz = -1 * D.y * wa * cv;
                float ny = 1 - Q * wa * sv;

                n += vec3(nx,ny,nz);
                n = normalize(n);


            }
     


            out V2F v2f;
            void main()
            {
                vec3 localPos = aPosition;
                vec3 normal = vec3(0.0);
                //vec2 D = localPos.xz - vec2(0.2);
                //SinWave( 10, 50, 50, 10,D,localPos,normal);


               int waveCount = int(_OceanGlobals.x);
               for(int i = 0; i < waveCount; ++i)
                {
                    OceanWave wave = _OceanWaves[i];
                    GerstnerWave(wave.params.x, wave.params.y, wave.params.z, wave.params.w, wave.direction.xy, localPos, normal);
                }



                vec3 worldpos = ObjectToWorldPos(localPos);

                v2f.worldpos = worldpos;
                v2f.worldnor = normal;
                v2f.CamVec = _CameraPosition - worldpos;

                gl_Position = WorldToClipPos(worldpos);
            }
            #endif

            #ifdef FRAGMENT

            in V2F v2f;
            out vec4 FragColor;
            void main() 
            {
                Surface s;
                s.basecolor = vec3(.8);
                s.position = v2f.worldpos;
                s.normal = normalize(v2f.worldnor);
                s.roughness = 0.2;
                s.metallic = 0.0;
                s.ao = 1.0;


                vec3 finalColor = CalcAllLight(s,normalize(v2f.CamVec));



                FragColor = vec4(finalColor,1.0);
            }
            #endif
            ENDGLSL
        }
        
    }
}