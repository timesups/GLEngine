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

uniform float _specular;

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

layout(binding = 10) uniform sampler2D shadowMap;
layout(location = 0) out vec4 outGBuffer0;


struct MaskSample {
    float r;          // AO / cavity
    float g;          // 混合权重（皮肤/细节）
    float b;          // 材质参数插值
    float a;          // Rim 遮罩
    float maskedAO;   // R * 视角项
    vec3  bentNormal; // 用 G 弯折后的法线
};


MaskSample processMaskMap(
    vec2  uv,
    vec3  worldNormal,
    vec3  viewDir,          // _558，已 normalize
    float viewSideY         // _603 = normalize(camForward.xz).y
) {
    vec4 m = texture(_sdfcontrol, uv); // ImplicitLod + Bias

    MaskSample o;
    o.r = m.r; // _524
    o.g = m.g; // _525
    o.b = m.b; // _526
    o.a = m.a; // _527

    // R：乘视角侧向衰减；G→1 时衰减消失
    float side = clamp(viewSideY + 0.5, 0.0, 1.0);
    float sideFactor = mix(side, 1.0, o.g);       // _1165
    o.maskedAO = o.r * sideFactor;                // _1166

    // G：弯折法线 —— mix(viewDir, flattenY(N), G)
    vec3 flatN = normalize(vec3(worldNormal.x, 0.0001, worldNormal.z));
    o.bentNormal = normalize(mix(viewDir, flatN, vec3(o.g))); // _655

    return o;
}


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
    ivec2 pixel = ivec2(gl_FragCoord.xy);
    float shadow  = texelFetch(shadowMap,pixel,0).x;

    Light mainLight = GetMainLight(normalWS);

    BRDFParam param = GetDefaultBRDFParam();

    param.pixelNormal = normalWS;
    param.baseColor = baseColor *_brightness;
    param.roughness = (1-attri.a) * _roughness;
    param.metallic = attri.r * _metallic;
    param.ao = attri.b;
    param.tangent = T;

    param.anisotropy = _Anisotropy;
    param.anisotropyShift = 0.0;
    param.anisotropyRotate = _anisotropyRotation;
    param.specular = _specular * attri.g;


    

    vec3 color = vec3(0.0);

    if(_shadingModel == 1 || _shadingModel == 5)
    {
        float lutSample = (dot(param.pixelNormal,mainLight.direction) + 1.0)* 0.5;
        lutSample = clamp(lutSample,0.4,0.8);
        vec3 NdotLLut = texture(_basecolorLUT,vec2(lutSample,0.0)).xyz * shadow;

        vec3 direct = PBR(mainLight,param,viewDir) * NdotLLut;

        vec3 ambient = CaclIBL(param,viewDir);
        color = direct + ambient;

    }
    else if(_shadingModel == 2)
    {
        param.roughness = 1.0;


        float lutSample = (dot(param.pixelNormal,mainLight.direction) + 1.0)* 0.5;
        lutSample = clamp(lutSample,0.4,0.8);
        vec3 NdotLLut = texture(_basecolorLUT,vec2(lutSample,0.0)).xyz * shadow;

        vec3 direct = PBR(mainLight,param,viewDir) * NdotLLut;

        vec3 ambient = CaclIBL(param,viewDir);
        color = direct + ambient;

    }
    else if(_shadingModel == 6)
    {
        MaskSample maskSample = processMaskMap(v2f.uv,param.pixelNormal,viewDir,normalize(_CameraForward.xz).y);

        mat3 worldToObject = mat3(inverse(GL_MATRIX_M));

        vec3 Lobj =  worldToObject * mainLight.direction;
        Lobj = normalize(vec3(Lobj.x,0.0001,Lobj.z));


        float flipU = Lobj.x > 0.0?1.0:0.0;

        float u = mix(1.0-v2f.uv.x,v2f.uv.x,flipU);
        vec4 f = textureLod(_facesdf,vec2(u,v2f.uv.y),0.0);

        float s = f.b * 2.0;
        float dir = mix(1-s,s-1,flipU);
        vec3 nLocal = normalize(vec3(dir,1.001,1.0-abs(dir)));

        vec3 nFace = normalize(transpose(worldToObject) * nLocal);
        param.pixelNormal = nFace;


        float camForwardDotLxz = dot(normalize(mainLight.direction.xz),normalize(_CameraForward.xz));
        float lz = Lobj.z;
        float lzNeg = clamp(-lz,0.0,1.0);
        float reshaped = (-lz) * (lz * 0.5 -1.0) + 0.5;
        float backCam = clamp(-camForwardDotLxz,0.0,1.0);

        float w = backCam * lzNeg * (1.0-1.0);//uFaceLightBlend
        float faceFactor = mix(lz,reshaped,w);

        float shadowRG = (f.r + f.g) *0.5;

        float t= faceFactor * 0.5;
        float lo = clamp(0.5-t,0.001,0.999);
        float hi = min(lo + lo,1.0);
        float edge0 = max(lo - (1.0 - lo),0.0);
        float soft = smoothstep(edge0,hi,shadowRG);

        float remapped = abs(-soft - t* ceil(t));
        float faceLit = mix(-1.0,1.0,remapped);

        float occlusionA = f.a;

        shadow = 1.0;
        param.roughness = 1.0;

        float NdotL = 1-dot(param.pixelNormal,-mainLight.direction);

        color = vec3(faceLit * baseColor);

    }

 

    outGBuffer0 = vec4(vec3(color), 1.0);


}



#endif

#endif
