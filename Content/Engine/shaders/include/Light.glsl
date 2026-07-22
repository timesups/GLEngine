#ifndef LIGHT_INCLUDE
#define LIGHT_INCLUDE

struct Light
{
    vec3 position;
    vec3 direction;

    vec3 color;
    float attenuation;

    float bias;
    float pcfSample;
    float zFar;
    float zNear;

    float index;

    bool isLoaclLight;
};

Light GetMainLight(Surface s)
{
    Light mainLight;
    mainLight.direction = normalize(-_MainLightDirection);
    // CPU: color × DirectionalLuxToShaderLi(lux)；组件强度单位为 lux（见 LightingConvention.h）
    mainLight.color = _MainLightColor;
    mainLight.attenuation = 1;
    mainLight.bias = max(_MainLightBias*10 * (1.0 - dot(s.normal, mainLight.direction)), _MainLightBias);
    mainLight.pcfSample = _MainLightPcfSample;
    mainLight.isLoaclLight = false;
    mainLight.position = vec3(0);
    return mainLight;
}


Light GetMainLight(vec3 normal)
{
    Light mainLight;
    mainLight.direction = normalize(-_MainLightDirection);
    // CPU: color × DirectionalLuxToShaderLi(lux)；组件强度单位为 lux（见 LightingConvention.h）
    mainLight.color = _MainLightColor;
    mainLight.attenuation = 1;
    mainLight.bias = max(_MainLightBias*10 * (1.0 - dot(normal, mainLight.direction)), _MainLightBias);
    mainLight.pcfSample = _MainLightPcfSample;
    mainLight.isLoaclLight = false;
    mainLight.position = vec3(0);
    return mainLight;
}


Light GetLocalLight(int index,Surface s)
{
    LocalLight l = _LocalLights[index];

    Light light;
    light.isLoaclLight = true;
    light.position = l.position;
    light.zFar = l.zFar;
    light.zNear = l.zNear;
    light.bias = l.bias;
    light.pcfSample = l.pcfSample;
    light.index = index;
    
    vec3 lightVec =  l.position - s.position;
    light.direction = normalize(lightVec);




    float dist = length(lightVec);

    float theta = dot(light.direction,normalize(-l.direction));
    float epsilon = l.inCutOff - l.outCutOff;
    float spotCone = clamp((theta - l.outCutOff)/(epsilon+0.00000001),0.0,1.0);

    // color = rgb × lumens；各向同性点光 E(lux) = lumens / (4π·dist²)
    light.attenuation = (1.0 / (4.0 * PI * dist * dist)) * spotCone;


    light.color = l.color;
    return light;
}

Light GetLocalLight(int index,vec3 position)
{
    LocalLight l = _LocalLights[index];

    Light light;
    light.isLoaclLight = true;
    light.position = l.position;
    light.zFar = l.zFar;
    light.zNear = l.zNear;
    light.bias = l.bias;
    light.pcfSample = l.pcfSample;
    light.index = index;
    
    vec3 lightVec =  l.position - position;
    light.direction = normalize(lightVec);




    float dist = length(lightVec);

    float theta = dot(light.direction,normalize(-l.direction));
    float epsilon = l.inCutOff - l.outCutOff;
    float spotCone = clamp((theta - l.outCutOff)/(epsilon+0.00000001),0.0,1.0);

    // color = rgb × lumens；各向同性点光 E(lux) = lumens / (4π·dist²)
    light.attenuation = (1.0 / (4.0 * PI * dist * dist)) * spotCone;


    light.color = l.color;
    return light;
}

int GetLocalLightCount()
{
    return int(_LocalLightCount);
}




#endif
