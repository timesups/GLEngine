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
            uniform samplerCube _cubeMap;
            uniform int _PhiSamples;
            uniform int _ThetaSamples;
            void main() 
            {
                vec3 N = normalize(locPos);

                vec3 irradiance = vec3(0.0);

                vec3 up = abs(N.y) < 0.999 ? vec3(0.0, 1.0, 0.0) : vec3(1.0, 0.0, 0.0);
                vec3 right = normalize(cross(up, N));
                up = normalize(cross(N, right));

                int phiSamples = max(_PhiSamples, 1);
                int thetaSamples = max(_ThetaSamples, 1);
                for (int phiIdx = 0; phiIdx < phiSamples; ++phiIdx)
                {
                    float phi = (float(phiIdx) + 0.5) / float(phiSamples) * 2.0 * PI;
                    for (int thetaIdx = 0; thetaIdx < thetaSamples; ++thetaIdx)
                    {
                        float theta = (float(thetaIdx) + 0.5) / float(thetaSamples) * 0.5 * PI;
                        vec3 tangentSample = vec3(sin(theta) * cos(phi), sin(theta) * sin(phi), cos(theta));
                        vec3 sampleVec = tangentSample.x * right + tangentSample.y * up + tangentSample.z * N;

                        irradiance += textureLod(_cubeMap, sampleVec, 0.0).rgb * cos(theta) * sin(theta);
                    }
                }
                irradiance = PI * irradiance / float(phiSamples * thetaSamples);
                FragColor = vec4(irradiance, 1.0);

            }
            #endif
            ENDGLSL
        }
    }
}
