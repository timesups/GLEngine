#ifndef ENDFIELD_FORWARD_PASS_INCLUDE
#define ENDFIELD_FORWARD_PASS_INCLUDE
#include "Core.glsl"
#include "EndfieldLibrary.glsl"

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

#include "Surface.glsl"
#include "Lighting.glsl"

uniform sampler2D _Color;
uniform sampler2D _Normal;
uniform sampler2D _Mask;
uniform sampler2D _Attribute;

uniform sampler2D _basecolorLUT;


uniform float _NormalScale;
uniform float _roughness;
uniform float _metallic;
uniform float _DoubleSided;

uniform int _shadingModel; // 0:衣服, 1:皮肤, 3:eye, 4:hair, 5:scene

uniform bool _IsCharacter;

layout(binding = 6) uniform sampler2D shadowMap;

layout(location = 0) out vec4 outGBuffer0;



vec3 sampleColorLUT(vec3 linearRgb) {
    float z = linearRgb.z * 31.0;
    float z0 = floor(z);
    vec2 xy = linearRgb.xy * 31.0;
    vec2 uv = xy * vec2(0.0009765625, 0.03125) + vec2(0.00048828125, 0.015625);
    uv.x += z0 * 0.03125;
    vec3 a = textureLod(_basecolorLUT, uv, 0.0).xyz;
    vec3 b = textureLod(_basecolorLUT, uv + vec2(0.03125, 0.0), 0.0).xyz;
    return mix(a, b, vec3(z - z0));
}





void main()
{
    vec4 mask = texture(_Mask, v2f.uv);
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

    vec4 attri = texture(_Attribute, v2f.uv);
    vec3 baseColor = texture(_Color, v2f.uv).rgb;
    vec3 viewDir = normalize(v2f.viewDir);

    //软边缘着色
    float NdVsoft = clamp(dot(normalWS,viewDir),0.0,1.0);
    float soft = clamp((1.0 - clamp(NdVsoft * 0.85+0.15,0.0,1.0)) * 2.0,0.0,1.0);
    baseColor = baseColor * mix(vec3(1.0),vec3(1,1,1),soft);


    //diffuse
    vec3 diffuse = baseColor * (0.96 - 0.96 * _metallic);
    vec3 F0 = mix(vec3(0.04) * _roughness,baseColor,vec3(_metallic));

    float a = max(_roughness * _roughness,0.0078125);
    float a2 = a * a;
    //太阳
    vec3 sunDir = normalize(-_MainLightDirection);
    vec3 sunDirFlat = normalize(vec3(sunDir.x,0.0001,sunDir.z));
    vec3 sunColor = _MainLightColor;
    ivec2 pixel = ivec2(gl_FragCoord.xy);
    vec4 shadow = texelFetch(shadowMap,pixel,0);




    vec3 finalColor = vec3(shadow.xyz);

    outGBuffer0 = vec4(finalColor,1.0);
}
#endif

#endif
