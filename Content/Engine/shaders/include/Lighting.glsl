#ifndef LIGHTING_INCLUDE
#define LIGHTING_INCLUDE
#include "BRDF.glsl"
#include "Shadow.glsl"

vec3 CalcIBLSpecular(Surface s, vec3 cameraVecWS, vec3 F0)
{
    vec3 R = reflect(-cameraVecWS, s.normal);
    // 与 prefilter mip 烘焙的线性 roughness 映射一致；高 roughness 段分配更多 mip 精度
    float lod = clamp(s.roughness, 0.0, 1.0) * _MaxReflectionLOD;
    vec3 prefilteredColor = textureLod(_prefiltered, R, lod).rgb;
    vec2 envBRDF = texture(_brdfLUT, vec2(max(dot(s.normal, cameraVecWS), 0.0), s.roughness)).rg;
    return prefilteredColor * (F0 * envBRDF.x + envBRDF.y);
}



vec3 CaclIBL(Surface s, vec3 CameraVecWS)
{

    vec3 IBLColor = vec3(0.0);
    vec3 F0 = vec3(0.04);
    F0 = mix(F0, s.basecolor, s.metallic);
    vec3 kS = fresnelSchlickRoughness(max(dot(s.normal, CameraVecWS), 0.0), F0, s.roughness);
    vec3 kD = (1.0 - kS) * (1.0 - s.metallic);
    if (_IBLLightingEnabled > 0.5)
    {
        //计算IBL漫反射部分
        vec3 irradiance = texture(_irradianceMap, s.normal).xyz * _IBLLightingIntensity;
        vec3 ambientColor = irradiance * s.basecolor * s.ao * kD;
        IBLColor += ambientColor;


        //计算IBL高光部分
        vec3 specularIBL = CalcIBLSpecular(s, CameraVecWS, F0);
        IBLColor += specularIBL * _IBLLightingIntensity * s.ao;
    }

    return IBLColor;
}



vec3 CalcAllLight(Surface s, vec3 CameraVecWS)
{
    Light mainLight = GetMainLight(s);
    //计算主光光照
    vec3 color = PBR(mainLight, s, CameraVecWS);
    float shadow = CalcShadow(mainLight, s);
    color = (1.0 - shadow) * color;

    //逐个计算局部光照
    for (int i = 0; i < min(_LocalLightCount, MAXLOCALLIGHT); i++)
    {
        Light light = GetLocalLight(i, s);
        vec3 lightColor = PBR(light, s, CameraVecWS);
        shadow = CalcShadow(light, s);
        lightColor = (1.0 - shadow) * lightColor;
        color += lightColor;
    }
    color += CaclIBL(s,CameraVecWS);
    return color;
}

#endif
