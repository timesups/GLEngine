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
            #include "include/Core.glsl"
            #ifdef VERTEX
            void main()
            {
                gl_Position = GetModelMatrix() * vec4(aPosition,1.0);
 
            }
            #endif
            #ifdef GEOMETRY
            // in gl_Vertex
            // {
            //     vec4  gl_Position;
            //     float gl_PointSize;
            //     float gl_ClipDistance[];
            // } gl_in[];  
            layout (triangles) in;
            layout (triangle_strip,max_vertices= MAXLOCALLIGHTVERTEX) out;
            out vec3 FragPos;
            void main()
            {
                for(int lightIndex=0;lightIndex < min(_LocalLightCount,MAXLOCALLIGHT);lightIndex++)
                {
                    vec3 lightPos = _LocalLights[lightIndex].position;
                    for(int face=0;face<6;++face)
                    {
                        gl_Layer = lightIndex*6+face;
                        mat4 lightMatrix = _LocalLights[lightIndex].matrix[face];

                        for(int i=0;i<3;i++)
                        {
                            vec4 pos = gl_in[i].gl_Position;
                            FragPos = pos.xyz;
                            gl_Position = lightMatrix * pos;
                            EmitVertex();

                        }
                        EndPrimitive();
                    }
                }
                

            }

            #endif

            #ifdef FRAGMENT
            in vec3 FragPos;
            void main() 
            {
                int lightIndex = gl_Layer / 6;
                vec3 lightPos = _LocalLights[lightIndex].position;

                // get distance between fragment and light source
                float lightDistance = length(FragPos - lightPos);
                // // map to [0;1] range by dividing by far_plane
                lightDistance = lightDistance/_LocalLights[lightIndex].zFar;
                // write this as modified depth
                gl_FragDepth = lightDistance;

            }
            #endif
            ENDGLSL
        }
    }
}