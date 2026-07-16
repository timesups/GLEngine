GLSLShader
{
    Properties
    {
        sampler2D _Color = "white"
        sampler2D _Normal = "normal"
        sampler2D _Mask = "white"
        sampler2D _Attribute = "grey"

        float _NormalScale = 1.0
        float _roughness = 0.5
        float _metallic = 0.0
        float _DoubleSided = 1.0

        float _OutLineWidth = 0.01
        vec3 _OutLineColor = (0,0,0)

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
            Stencil 
            {
                BitMask 0xff
                AndMask 0xff
                Func always
                Ref 0x34
                fail keep
                dpfail keep 
                dppass replace
            }
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
            Stencil 
            {
                BitMask 0xff
                AndMask 0xff
                Func always
                Ref 0x24
                fail keep
                dpfail keep 
                dppass keep
            }
            ztest lequal
            GLSLPROGRAM
            #include "Core.glsl"
            #include "EndfieldLibrary.glsl"
            #include "EndfieldForwardPass.glsl"
            ENDGLSL
        }
    }
}
