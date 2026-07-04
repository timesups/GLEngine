#ifndef BRDF_INCLUDE
#define BRDF_INCLUDE
#include "Surface.glsl"
#include "Light.glsl"


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


vec3 PBR(Light l,Surface s,vec3 CamVec)
{

    vec3 h = normalize(l.direction + CamVec);
    float a = s.roughness * s.roughness;
    float k = (a+1) * (a+1)/8.0;

    float NdotV = max(dot(s.normal,CamVec),0.0);
    float NdotL = max(dot(s.normal,l.direction),0.0);
    float HdotV = max(dot(h,CamVec),0.0);

    float D = DistributionGGX(s.normal,h,a);
    float G = GeometrySmith(s.normal,CamVec,l.direction,k);
    vec3 F0 = vec3(0.04);
    F0 = mix(F0,s.basecolor,s.metallic);
    vec3 F = fresnelSchlick(HdotV,F0);
    vec3 nom = D*G*F;
    float denom = max(4 * NdotV * NdotL,0.0001);
    vec3 brdfCookTorr = nom/denom; 

    vec3 kd = vec3(1.0) - F;
    kd *= 1.0 - s.metallic;//将金属表面的漫反射强制归零

    vec3 L = (kd * s.basecolor/PI + brdfCookTorr) * l.color *  l.attenuation * NdotL;

    return L;
}

//blinn phong
vec3 BlinnPhone(Light l,Surface s,vec3 CameraVecWS)
{
    //diffuse
    vec3 diffuse = max(dot(l.direction,s.normal),0.0) * s.basecolor * l.color * l.attenuation;
    //specular
    vec3 halfVec = normalize(CameraVecWS + l.direction);
    vec3 specular = pow(max(dot(halfVec,s.normal),0.0),256*(1-s.roughness))* l.color * l.attenuation;
    return diffuse+specular;
}


#endif
