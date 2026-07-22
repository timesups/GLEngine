#ifndef ENDFIELD_FORWARD_PASS_INCLUDE
#define ENDFIELD_FORWARD_PASS_INCLUDE
#include "Core.glsl"
#include "EndfieldLibrary.glsl"


uniform sampler2D _Color;
uniform sampler2D _Normal;
uniform sampler2D _Mask;
uniform sampler2D _Attribute;
uniform sampler2D _sdfcontrol;
uniform sampler2D _facesdf;

uniform sampler2D _basecolorLUT;


uniform float _NormalScale;
uniform float _Anisotropy;
uniform float _anisotropyRotation;

uniform float _brightness;


uniform float _roughness;
uniform float _metallic;
    


uniform int _shadingModel; // 1:衣服, 2:皮肤, 3:eye, 4:hair, 5:scene,6:face
uniform bool _IsCharacter;

#include "EndfieldPBR.glsl"

struct V2f
{
    vec2 uv;
    vec3 normalWS;
    vec4 tangentWS;
    vec3 positionWS;
    vec3 viewDir;
};

#ifdef VERTEX
out V2f v2f;
void main()
{
    uint packedNormal = floatBitsToUint(aNormal.x);
    bool isPacked = (packedNormal & 0x40000000u) > 0u;

    vec3 N;
    vec4 T;
    if (isPacked)
        unpackNormalTangent(packedNormal, N, T);
    else
    {
        N = aNormal;
        T = aTangent;
    }

    vec3 worldPos = ObjectToWorldPos(aPosition);
    gl_Position = WorldToClipPos(worldPos);

    v2f.normalWS = ObjectToWorldN(N);
    vec3 tangentWS = ObjectToWorldN(T.xyz);
    tangentWS = normalize(tangentWS - dot(tangentWS, v2f.normalWS) * v2f.normalWS);
    v2f.tangentWS = vec4(tangentWS, T.w);
    v2f.uv = aTexcoord0;
    v2f.positionWS = worldPos;
    v2f.viewDir = _CameraPosition - v2f.positionWS;
}
#endif

#ifdef FRAGMENT
in V2f v2f;

layout(binding = 6) uniform sampler2D shadowMap;
layout(location = 0) out vec4 outGBuffer0;

void main()
{
    vec4 mask = texture(_Mask, v2f.uv);
    if (mask.x < 0.3333)
        discard;
    vec3 geomNormal = normalize(v2f.normalWS);
    vec3 T = normalize(v2f.tangentWS.xyz);
    vec3 B = cross(geomNormal, T) * v2f.tangentWS.w;
    mat3 TBN = mat3(T, B, geomNormal);
    vec3 normalTS = unpackNormalMapWY(_Normal, v2f.uv, _NormalScale);
    vec3 normalWS = normalize(TBN * normalTS);
    float backFaceSign = -1.0 + 2.0;
    normalWS *= gl_FrontFacing ? 1.0 : backFaceSign;

    vec4 attri = texture(_Attribute, v2f.uv);
    vec3 baseColor = texture(_Color, v2f.uv).rgb;
    vec3 viewDir = normalize(v2f.viewDir);
    float faceSign = gl_FrontFacing ? 1.0 : -1.0;
    normalWS *= faceSign;
    //主光阴影
    // ivec2 pixel = ivec2(gl_FragCoord.xy);
    // float shadow  = texelFetch(shadowMap,pixel,0).x;

    Light mainLight = GetMainLight(normalWS);

    BRDFParam param = GetDefaultBRDFParam();

    if(_shadingModel == 1)
    {
        param.pixelNormal = normalWS;
        param.baseColor = baseColor *_brightness;
        param.roughness = (1-attri.a) * _roughness;
        param.metallic = attri.r * _metallic;
        param.ao = attri.b;
        param.tangent = T;

        param.anisotropy = _Anisotropy;
        param.anisotropyShift = 0.0;
        param.anisotropyRotate = _anisotropyRotation;


        vec3 direct = PBR(mainLight,param,viewDir);
        vec3 ambient = CaclIBL(param,viewDir);
        vec3 color = direct + ambient;
        outGBuffer0 = vec4(vec3(direct), 1.0);
    }
    else
    {
         outGBuffer0 = vec4(0.1);
    }



}



#endif

#endif
