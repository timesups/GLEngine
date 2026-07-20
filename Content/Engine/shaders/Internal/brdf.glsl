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
            // 烘焙专用：不 include Core.glsl，避免 _brdfLUT 与 FBO 输出反馈循环
            #define PI 3.141592654

            const uint SAMPLE_COUNT = 1024u;

            float RadicalInverse_VdC(uint bits)
            {
                bits = (bits << 16u) | (bits >> 16u);
                bits = ((bits & 0x55555555u) << 1u) | ((bits & 0xAAAAAAAAu) >> 1u);
                bits = ((bits & 0x33333333u) << 2u) | ((bits & 0xCCCCCCCCu) >> 2u);
                bits = ((bits & 0x0F0F0F0Fu) << 4u) | ((bits & 0xF0F0F0F0u) >> 4u);
                bits = ((bits & 0x00FF00FFu) << 8u) | ((bits & 0xFF00FF00u) >> 8u);
                return float(bits) * 2.3283064365386963e-10;
            }

            vec2 Hammersley(uint i, uint N)
            {
                return vec2(float(i) / float(N), RadicalInverse_VdC(i));
            }

            vec3 ImportanceSampleGGX(vec2 Xi, vec3 N, float roughness)
            {
                float a = roughness * roughness;
                float phi = 2.0 * PI * Xi.x;
                float cosTheta = sqrt((1.0 - Xi.y) / (1.0 + (a * a - 1.0) * Xi.y));
                float sinTheta = sqrt(1.0 - cosTheta * cosTheta);
                vec3 H = vec3(cos(phi) * sinTheta, sin(phi) * sinTheta, cosTheta);

                vec3 up = vec3(0.0, 1.0, 0.0);
                vec3 tangent = normalize(cross(up, N));
                vec3 bitangent = cross(N, tangent);
                return normalize(tangent * H.x + bitangent * H.y + N * H.z);
            }

            float GeometrySchlickGGX(float NdotV, float roughness)
            {
                float a = roughness;
                float k = (a * a) / 2.0;
                float nom = NdotV;
                float denom = NdotV * (1.0 - k) + k;
                return nom / max(denom, 0.0001);
            }

            vec2 IntegrateBRDF(float NdotV, float roughness)
            {
                vec3 V;
                V.x = sqrt(1.0 - NdotV * NdotV);
                V.y = 0.0;
                V.z = NdotV;

                float A = 0.0;
                float B = 0.0;
                vec3 N = vec3(0.0, 0.0, 1.0);

                for (uint i = 0u; i < SAMPLE_COUNT; ++i)
                {
                    vec2 Xi = Hammersley(i, SAMPLE_COUNT);
                    vec3 H = ImportanceSampleGGX(Xi, N, roughness);
                    vec3 L = normalize(2.0 * dot(V, H) * H - V);

                    float NdotL = max(L.z, 0.0);
                    float NdotH = max(H.z, 0.0);
                    float VdotH = max(dot(V, H), 0.0);

                    if (NdotL > 0.0)
                    {
                        float G = GeometrySchlickGGX(NdotV, roughness) * GeometrySchlickGGX(NdotL, roughness);
                        float G_Vis = (G * VdotH) / max(NdotH * NdotV, 0.0001);
                        float Fc = pow(1.0 - VdotH, 5.0);
                        A += (1.0 - Fc) * G_Vis;
                        B += Fc * G_Vis;
                    }
                }

                return vec2(A, B) / float(SAMPLE_COUNT);
            }

            #ifdef VERTEX
            layout(location = 0) in vec3 aPosition;
            out vec2 uv;
            void main()
            {
                gl_Position = vec4(aPosition.xy, 0.0, 1.0);
                uv = aPosition.xy * 0.5 + 0.5;
            }
            #endif

            #ifdef FRAGMENT
            in vec2 uv;
            out vec4 FragColor;
            void main() 
            {
                vec2 integratedBRDF = IntegrateBRDF(max(uv.x, 0.0001), uv.y);
                FragColor = vec4(integratedBRDF, 0.0, 1.0);
            }
            #endif
            ENDGLSL
        }
    }
}
