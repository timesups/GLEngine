GLSLShader
{
    Properties
    {
        sampler2D _Color = "white"
        float _brightness = 1.0



        sampler2D _Normal = "normal"
        float _NormalScale = 1.0

        sampler2D _Mask = "white"
        sampler2D _Attribute = "grey"
        sampler2D _basecolorLUT = "white"

        sampler2D _facesdf = "black"
        sampler2D _sdfcontrol = "black"



        float _Anisotropy = 0.0
        float _anisotropyRotation = 0.0


        float _metallic = 1.0
        float _roughness = 1.0
    

        int _shadingModel = 0
        bool _IsCharacter

    }
    SubShader
    {
        Pass
        {
            Tags
            {
                LightMode Deferred
            }
            cull front
            GLSLPROGRAM
            #include "Core.glsl"
            #include "EndfieldLibrary.glsl"
            #include "EndfieldDefrredPass.glsl"
            ENDGLSL
        }

        Pass
        {
            Tags
            {
                LightMode Forward
            }
            cull front
            ztest lequal
            GLSLPROGRAM
            #include "EndfieldForwardPass.glsl"
            ENDGLSL
        }

        Pass
        {
            Tags
            {
                LightMode Outline
            }
            GLSLPROGRAM
            #include "EndfieldOutLinePass.glsl"
            ENDGLSL
        }
    }
}
