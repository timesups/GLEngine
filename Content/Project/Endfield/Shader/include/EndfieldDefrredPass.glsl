#ifndef ENDFIELD_DEFAULT_INCLUDE
#define ENDFIELD_DEFAULT_INCLUDE


struct V2f
{
    vec2 uv;
    vec3 normalWS;
    vec4 tangentWS;
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
}
#endif

#ifdef FRAGMENT
in V2f v2f;

uniform sampler2D _Color;
uniform sampler2D _Normal;
uniform sampler2D _Mask;
uniform sampler2D _Attribute;

uniform float _NormalScale;
uniform float _roughness;
uniform float _metallic;
uniform float _DoubleSided;

uniform int _shadingModel; //0:衣服,1:皮肤,3:eye,4hair,5:scene

uniform bool _IsCharacter;

layout(location = 0) out vec4 outGBuffer0;
layout(location = 1) out vec4 outGBuffer1;
layout(location = 2) out vec4 outGBuffer2;
layout(location = 3) out vec4 outGBuffer3;
layout(location = 4) out vec4 outGBuffer4;

vec4 packMaterialAttributes(vec2 uv)
{
    vec4 attri = texture(_Attribute, uv);
    return vec4(_metallic, _roughness, attri.r, attri.g * 0.3333);
}
void main()
{
    vec4 mask = texture(_Mask,v2f.uv);
    if (mask.x < 0.3333)
        discard;

    vec3 N = normalize(v2f.normalWS);
    vec3 T = normalize(v2f.tangentWS.xyz);
    vec3 B = cross(N, T) * v2f.tangentWS.w;
    mat3 TBN = mat3(T, B, N);

    vec3 normalTS = unpackNormalMapWY(_Normal, v2f.uv, _NormalScale);
    vec3 normalWS = normalize(TBN * normalTS);

    float backFaceSign = -1.0 + 2.0 * _DoubleSided;
    normalWS *= gl_FrontFacing ? 1.0 : backFaceSign;

    vec3 baseColor = texture(_Color, v2f.uv).rgb;
    vec2 normalEnc = encodeNormalEndfield(normalWS);


    float buffer1a = 0.3333;
    float buffer3a = 0.0;
    float buffer3b = 0.0;


    if(_shadingModel == 5)
    {
        buffer1a = 0.0;
        buffer3b = 1.0;
    }
    else if(_shadingModel == 1)
    {
        buffer3a = 0.3333;
    }
    else if(_shadingModel == 3)
    {
        buffer3a = 0.6666;
    }
    else if(_shadingModel == 4)
    {
        buffer3a = 1.0;
    }

    float buffer4a = 0.0;
    if(_IsCharacter)
    {
        buffer4a = 1.0;
    }

    


    outGBuffer0 = vec4(0.0);
    outGBuffer1 = vec4(0.0, 0.0, 1.0, buffer1a);
    outGBuffer2 = packMaterialAttributes(v2f.uv);
    outGBuffer3 = vec4(normalEnc, buffer3b, buffer3a);
    outGBuffer4 = vec4(baseColor,buffer4a);
}
#endif


#endif