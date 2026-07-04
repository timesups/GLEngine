GLSLShader
{
    Properties
    {

    }
    SubShader
    {
        Pass
        {
            queue transparent
            blend srcalpha oneminussrcalpha
            cull off
            GLSLPROGRAM
            #include "Core.glsl"
            #include "Light.glsl"

            struct V2F
            {
                vec3 NormalWS;
                vec3 PositionWS;
            };

            #ifdef VERTEX
            out V2F v2f;
            void main()
            {
                gl_Position = ObjectToClipPos(aPosition);
                v2f.NormalWS = ObjectToWorldN(aNormal);
                v2f.PositionWS = ObjectToWorldPos(aPosition);
            }
            #endif

            #ifdef FRAGMENT
            in V2F v2f;
            out vec4 FragColor;

            layout (binding=10) uniform sampler2D _WaterVolume;


            void main()
            {
                vec2 uv = gl_FragCoord.xy / vec2(_ScreenWidth, _ScreenHeight);
                vec4 NormalXY= texture(_WaterVolume,uv);

                float backDepth = NormalXY.z;
                vec3 backNormal = DecodeNormalOct(NormalXY.xy);
                vec3 backPos = ReconstructPositionWS(uv,backDepth);

                Surface s;
                Light mainLight = GetMainLight(s);

                vec3 p = v2f.PositionWS;

                vec3 rayDir = normalize(p - _CameraPosition);
                vec3 geoNormal = normalize(normalize(v2f.NormalWS));
                geoNormal = faceforward(geoNormal, rayDir, geoNormal);
                float viewThickness = length(backPos - p);




                FragColor = vec4(vec3(1), viewThickness);
            }
            #endif
            ENDGLSL
        }
    }
}
