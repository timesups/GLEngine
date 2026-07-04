GLSLShader
{
    Properties
    {
        sampler2D _baseColor = "white"
        sampler2D _ao = "white"
        sampler2D _normal = "normal"
        sampler2D _roughness = "grey"
        float _rouMul = 1.0

        sampler2D _metallic = "grey"
        float _metaMul = 0.0
        
        int _ShadingModel = 1
    }
    SubShader
    {
        Pass
        { 
            cull back
            GLSLPROGRAM
            #include "Core.glsl"
            #include "Lighting.glsl"


            struct V2F
            {
                vec3 NormalWS;
                vec3 TangentWS;
                vec3 BNormalWS;
                vec3 PositionWS;
                vec3 CamVectorWS;
                vec2 uv;
                #ifdef FORWARDRENDER
                    vec4 posMainLight;
                #endif

            };

            #ifdef VERTEX
            out V2F v2f;
            void main()
            {
                gl_Position = ObjectToClipPos(aPosition);
                v2f.NormalWS = ObjectToWorldN(aNormal);

                v2f.TangentWS = ObjectToWorldN(aTangent.xyz);


                v2f.TangentWS = normalize(v2f.TangentWS-dot(v2f.TangentWS,v2f.NormalWS) * v2f.NormalWS);
                v2f.BNormalWS = cross(v2f.NormalWS,v2f.TangentWS) * aTangent.w;
                v2f.PositionWS = ObjectToWorldPos(aPosition);
                v2f.CamVectorWS = _CameraPosition - v2f.PositionWS;
                v2f.uv = aTexcoord0;


                #ifdef FORWARDRENDER
                    v2f.posMainLight = _MainLightMatrix * GetModelMatrix() * vec4(aPosition,1.0);
                #endif
            }
            #endif


            #ifdef FRAGMENT


            in V2F v2f;
            uniform sampler2D _baseColor;
            uniform sampler2D _ao;
            uniform sampler2D _normal;
            uniform sampler2D _roughness;
            uniform sampler2D _metallic;
            uniform float _metaMul;
            uniform float _rouMul;
            

            uniform int _ShadingModel;


            #ifdef FORWARDRENDER
                out vec4 FragColor;
            #endif

            #ifdef DEFERREDRENDER
                layout (location = 0) out vec4 _AlbdeoAO;
                layout (location = 1) out vec4  _NormalXY;
                layout (location = 2) out vec4  _MRSC;
                layout (location = 3) out vec4  _Flag;
            #endif



            void main() 
            {
                //vectors
                vec3 NormalWS = normalize(v2f.NormalWS);
                vec3 TangentWS = normalize(v2f.TangentWS);
                vec3 BNormalWS = normalize(v2f.BNormalWS);
                
                vec3 PositionWS = v2f.PositionWS;

                //texture sample
                vec3 BaseColor = texture(_baseColor, v2f.uv).xyz;
                float Roughness = texture(_roughness, v2f.uv).x *_rouMul;
                vec3 normalTS = texture(_normal, v2f.uv).xyz * 2.0 - 1.0;
                float ao = texture(_ao, v2f.uv).x;
                float metallic = texture(_metallic,v2f.uv).x * _metaMul;

                mat3 TBN = mat3(TangentWS, BNormalWS, NormalWS);
                NormalWS = normalize(TBN * normalTS);


                #ifdef FORWARDRENDER
                    Surface s;
                    s.basecolor = BaseColor;
                    s.normal = NormalWS;
                    s.position = PositionWS;
                    s.roughness = saturate(Roughness);
                    s.metallic = saturate(metallic);
                    s.posMainLight = v2f.posMainLight;
                    s.ao = ao;
                    vec3 CameraVecWS = normalize(v2f.CamVectorWS);
                    vec3 color = CalcAllLight(s,CameraVecWS);

                    FragColor = vec4(vec3(color), 1.0);
                #endif


                #ifdef DEFERREDRENDER
                    _AlbdeoAO = vec4(BaseColor,ao);
                    _NormalXY = vec4(EncodeNormalOct(NormalWS),0,0);
                    _MRSC = vec4(metallic, Roughness, 0.0, 0.0);
                    _Flag = vec4(_ShadingModel/255.0,1,1,1);
                #endif

            }
            #endif
            ENDGLSL
        }
    }
}