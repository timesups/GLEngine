#ifndef ENDFIELD_PBR_INCLUDE
#define ENDFIELD_PBR_INCLUDE
#include "Surface.glsl"
#include "Light.glsl"
#include "LightInput.glsl"



struct BRDFParam
{
    vec3 baseColor;
    vec3 pixelNormal;
    vec3 tangent;
    float roughness;
    float metallic;
    float ao;

    float specular;

    float anisotropy;
    float anisotropyShift;
    float anisotropyRotate;

};



BRDFParam GetDefaultBRDFParam()
{
    BRDFParam p;
    p.baseColor = vec3(1);
    p.pixelNormal = vec3(0,0,1);
    p.tangent = vec3(1,0,0);
    p.anisotropy = 0.0;
    p.anisotropyShift = 0.0;
    p.anisotropyRotate = 0.0;
    p.roughness = 0.5;
    p.metallic = 0.0;

    p.specular = 1.0;

    return p;
}



void rotateAnisotropyFrame(vec3 N, inout vec3 T, out vec3 B, float angle)
{
    // 1) 先保证 T 严格垂直于 N（法线贴图后 T 可能不再正交）
    T = normalize(T - N * dot(N, T));
    // 2) 原副切线
    vec3 B0 = cross(N, T);
    // 3) 切平面内 2D 旋转（等价于绕 N 的罗德里格斯）
    float c = cos(angle);
    float s = sin(angle);
    vec3 Tr = T * c + B0 * s;
    B       = cross(N, Tr);   // = -T*s + B0*c，重新算副切线
    T       = Tr;
}


float DistributionGGXAnisotropic(vec3 N,vec3 T,vec3 H,float roughness,float anisotropy,float angle)
{
    vec3 B;

    rotateAnisotropyFrame(N,T,B,angle);

    float alpha = max(roughness * roughness,0.0078);

    float alphaT = alpha * (1.0 + anisotropy);
    float alphaB = alpha * (1.0 - anisotropy);

    float NoH = dot(N,H);
    float ToH = dot(T,H);
    float BoH = dot(B,H);

    float alphaTB = alphaT * alphaB;

    vec3 v = vec3(
        alphaB * ToH,
        alphaT * BoH,
        alphaTB * NoH
    );

    float denminator = dot(v,v);
    denminator *= denminator;

    return (alphaTB * alphaTB * alphaTB)/max(denminator,1e-8);
}


float DistributionGGX(vec3 N, vec3 H, float a)
{
    float a2     = a*a;
    float NdotH  = max(dot(N, H), 0.0);
    float NdotH2 = NdotH*NdotH;

    float num   = a2;
    float denom = (NdotH2 * (a2 - 1.0) + 1.0);
    denom = PI * denom * denom;

    return num / denom;
}


float GeometrySchlickGGX(float NdotV,float k)
{
    float nom = NdotV;
    float denom = NdotV * (1.0 - k) + k;

    return nom / denom;
}

float GeometrySmith(vec3 N,vec3 V,vec3 L,float k)
{
    float NdotV = max(dot(N,V),0.0);
    float NdotL = max(dot(N,L),0.0);
    float ggx1 = GeometrySchlickGGX(NdotV,k);
    float ggx2 = GeometrySchlickGGX(NdotL,k);

    return ggx1 * ggx2;
}
vec3 fresnelSchlick(float cosTheta, vec3 F0)
{
    return F0 + (1.0 - F0) * pow(clamp(1.0 - cosTheta, 0.0, 1.0), 5.0);
}  

vec3 fresnelSchlickRoughness(float cosTheta, vec3 F0, float roughness)
{
    return F0 + (max(vec3(1.0 - roughness), F0) - F0) * pow(clamp(1.0 - cosTheta, 0.0, 1.0), 5.0);
}




vec3 PBR(Light l,BRDFParam p,vec3 viewDir)
{
    vec3 h = normalize(l.direction + viewDir);
    float a = p.roughness * p.roughness;
    float k = (a+1) * (a+1)/8.0;

    float NdotV = max(dot(p.pixelNormal,viewDir),0.0);
    float NdotL = max(dot(p.pixelNormal,l.direction),0.0);
    float HdotV = max(dot(h,viewDir),0.0);

    float D = DistributionGGX(p.pixelNormal,h,a);

    vec3 H_aniso = normalize(h + viewDir * p.anisotropyShift);
    float D_aniso = DistributionGGXAnisotropic(p.pixelNormal,p.tangent,H_aniso,p.roughness, p.anisotropy,p.anisotropyRotate);
    
    
    D = mix(D,D_aniso,p.anisotropy);


    float G = GeometrySmith(p.pixelNormal,viewDir,l.direction,k);
    vec3 F0 = vec3(0.04);
    F0 = mix(F0,p.baseColor,p.metallic);
    vec3 F = fresnelSchlick(HdotV,F0);
    vec3 nom = D*G*F;
    float denom = max(4 * NdotV * NdotL,0.0001);
    vec3 brdfCookTorr = nom/denom * p.specular; 

    vec3 kd = vec3(1.0) - F;
    kd *= 1.0 - p.metallic;//将金属表面的漫反射强制归零





    vec3 L = (kd * p.baseColor/PI + brdfCookTorr) * l.color *  l.attenuation  * NdotL;

    return L;
}




vec3 CalcIBLSpecular(BRDFParam p, vec3 cameraVecWS, vec3 F0)
{
    vec3 R = reflect(-cameraVecWS, p.pixelNormal);
    // 与 prefilter mip 烘焙的线性 roughness 映射一致；高 roughness 段分配更多 mip 精度
    float lod = clamp(p.roughness, 0.0, 1.0) * _MaxReflectionLOD;
    vec3 prefilteredColor = textureLod(_prefiltered, R, lod).rgb;
    vec2 envBRDF = texture(_brdfLUT, vec2(max(dot(p.pixelNormal, cameraVecWS), 0.0), p.roughness)).rg;
    return prefilteredColor * (F0 * envBRDF.x + envBRDF.y);
}



vec3 CaclIBL(BRDFParam p, vec3 CameraVecWS)
{

    vec3 IBLColor = vec3(0.0);
    vec3 F0 = vec3(0.04);
    F0 = mix(F0, p.baseColor, p.metallic);
    vec3 kS = fresnelSchlickRoughness(max(dot(p.pixelNormal, CameraVecWS), 0.0), F0, p.roughness);
    vec3 kD = (1.0 - kS) * (1.0 - p.metallic);
    if (_IBLLightingEnabled > 0.5)
    {
        //计算IBL漫反射部分
        vec3 irradiance = texture(_irradianceMap, p.pixelNormal).xyz * _IBLLightingIntensity;
        vec3 ambientColor = irradiance * p.baseColor * p.ao * kD;
        IBLColor += ambientColor;


        //计算IBL高光部分
        vec3 specularIBL = CalcIBLSpecular(p, CameraVecWS, F0);
        IBLColor += specularIBL * _IBLLightingIntensity * p.ao;
    }

    return IBLColor;
}


#endif