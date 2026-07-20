#ifndef ENDFIELD_FORWARD_PASS_INCLUDE
#define ENDFIELD_FORWARD_PASS_INCLUDE
#include "Core.glsl"
#include "EndfieldLibrary.glsl"
#include "LightInput.glsl"

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

const vec3 FaceDirection = vec3(0,0,1);

uniform sampler2D _Color;
uniform sampler2D _Normal;
uniform sampler2D _Mask;
uniform sampler2D _Attribute;
uniform sampler2D _sdfcontrol;
uniform sampler2D _facesdf;

uniform sampler2D _basecolorLUT;


uniform float _NormalScale;
uniform float _roughness;
uniform float _metallic;
uniform float _DoubleSided;


uniform int _shadingModel; // 0:衣服, 1:皮肤, 3:eye, 4:hair, 5:scene,6:face

uniform bool _IsCharacter;

layout(binding = 6) uniform sampler2D shadowMap;

layout(location = 0) out vec4 outGBuffer0;

float D_GGX(float NdotH, float a2) {
    float d = (NdotH * a2 - NdotH) * NdotH + 1.0;
    return a2 / max(d * d, 1e-7);
}

vec3 F_Schlick(vec3 F0, float VoH) {
    float f = pow(1.0 - VoH, 5.0);
    return F0 + (1.0 - F0) * f;
}

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

float V_SmithGGX(float NdotV, float NdotL, float a) {
    float gv = NdotL * (NdotV * (1.0 - a) + a);
    float gl = NdotV * (NdotL * (1.0 - a) + a);
    return 0.5 / max(gv + gl, 1e-5);
}

vec3 iblDiffuse(vec3 N, vec3 albedo, float metallic, vec3 F, float ao) {
    vec3 kD = (1.0 - F) * (1.0 - metallic);
    vec3 irradiance = texture(_irradianceMap, N).xyz * _IBLLightingIntensity;
    return kD * albedo * irradiance * ao;
}

vec3 F_SchlickRoughness(vec3 F0, float NdotV, float roughness) {
    float f = pow(1.0 - NdotV, 5.0);
    return F0 + (max(vec3(1.0 - roughness), F0) - F0) * f;
}

vec3 iblSpecular(vec3 N, vec3 V, vec3 F0, float roughness, float ao) {
    float NdotV = max(dot(N, V), 0.0);
    vec3 R = reflect(-V, N);
    float mip = clamp(roughness, 0.0, 1.0) * _MaxReflectionLOD;
    vec3 prefiltered = textureLod(_prefiltered, R, mip).rgb;
    vec2 brdf = texture(_brdfLUT, vec2(max(dot(N, V), 0.0), roughness)).rg;
    vec3 F = F_SchlickRoughness(F0, NdotV, roughness);
    return prefiltered * (F * brdf.x + brdf.y) * ao;
}



vec4 sampleFaceSDF(vec2 uv, mat3 worldFromTangent, vec3 L) {
    vec3 Lts = transpose(worldFromTangent) * L;
    Lts = normalize(vec3(Lts.x, 0.0001, Lts.z));
    float flip = Lts.x > 0.0 ? 1.0 : 0.0;
    vec2 sdfUV = vec2(mix(1.0 - uv.x, uv.x, flip), uv.y);

    vec4 sdf = texture(_facesdf, sdfUV);
    float dirX = mix(1.0 - sdf.z * 2.0, sdf.z * 2.0 - 1.0, flip);
    float dirZ = 1.0 - abs(dirX);
    vec3 featureTS = normalize(vec3(dirX, 0.0001, dirZ));
    vec3 featureWS = normalize(worldFromTangent * featureTS);
    return vec4(featureWS, sdf.w);
}

// 半兰伯特 wrap 漫反射（无专用 scalar LUT，用 half-lambert 近似）
float faceWrapDiffuse(vec3 N, vec3 L, float strength) {
    float u = clamp(dot(N, L), -1.0, 1.0) * 0.5 + 0.5;
    return mix(max(dot(N, L), 0.0), u, strength);
}

// 用 _basecolorLUT 作为 NdotL ramp 色调（对应参考的 wrapTint），按亮度归一
vec3 faceWrapTint(vec3 N, vec3 L) {
    float u = clamp(dot(N, L), -1.0, 1.0) * 0.5 + 0.5;
    vec3 tint = texture(_basecolorLUT, vec2(u, 0.5)).rgb;
    float lum = max(max(tint.r, tint.g), tint.b);
    return mix(vec3(1.0), tint / max(lum, 1e-3), step(0.001, lum));
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

    float faceSign = gl_FrontFacing ? 1.0 : -1.0;

    //软边缘着色
    float NdotV = clamp(dot(normalWS,viewDir),0.0,1.0);
    float soft = clamp((1.0 - clamp(NdotV * 0.85+0.15,0.0,1.0)) * 2.0,0.0,1.0);
    baseColor = baseColor * mix(vec3(1.0),vec3(1,1,1),soft);


    //diffuse
    vec3 diffuse = baseColor * (0.96 - 0.96 * _metallic);
    vec3 F0 = mix(vec3(0.04) * _roughness,baseColor,vec3(_metallic));
    //主光阴影
    ivec2 pixel = ivec2(gl_FragCoord.xy);
    float shadow  = texelFetch(shadowMap,pixel,0).x;
    Surface s;
    //主方向光
    Light mainLight = GetMainLight(s);
    vec3 L = normalize(mainLight.direction);
    vec3 H = normalize(viewDir + L);
    float NdotL_remap = (dot(N, L) + 1) * 0.5;
    vec3 lutColor = texture(_basecolorLUT,vec2(NdotL_remap,0.0)).xyz;

    if(_shadingModel == 6)
    {
        vec4 sdfCon = texture(_sdfcontrol,v2f.uv);
        vec2 sunDirPlane = vec2(L.x,L.z);
        float faceShade = dot(sunDirPlane,FaceDirection.xz);

        float sdfx = texture(_facesdf,v2f.uv).x;
        faceShade = smoothstep(sdfx,0.0,_metallic);

        
        if (_metallic < 0.0)
        {
            sdfx = texture(_facesdf,v2f.uv * vec2(-1,1)).x;
            faceShade = 1-smoothstep(sdfx,0.0,-_metallic);

        }
            


        outGBuffer0 = vec4(vec3(faceShade), 1.0);
    }
    else
    {
        float NdotL = max(dot(N, L), 0.0);
        float NdotH = max(dot(N, H), 0.0);
        float VdotH = max(dot(viewDir, H), 0.0);
            //ibl
        vec3 F_ibl = F_SchlickRoughness(F0,NdotV,_roughness);
        vec3 ambient = iblDiffuse(normalWS,diffuse,_metallic,F_ibl,1.0)
                        +iblSpecular(normalWS,viewDir,F0,_roughness,1.0);


        float a  = _roughness * _roughness;
        float a2 = a * a;
        float D  = D_GGX(NdotH, a2);
        float Vis = V_SmithGGX(NdotV, NdotL, a);
        vec3  F  = F_Schlick(F0, VdotH);





        vec3 spec = D * Vis * F;
        vec3 kD   = (1.0 - F) * (1.0 - _metallic);
        vec3 direct = (kD * diffuse / PI + spec) * mainLight.color * NdotL;
        

        vec3 finalColor =vec3(direct + ambient);




        outGBuffer0 = vec4(direct,1.0);
}
    }



#endif

#endif
