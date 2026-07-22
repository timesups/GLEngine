#ifndef SURFACE_INCLUDE
#define SURFACE_INCLUDE

struct Surface
{
    vec3 basecolor;
    vec3 normal;
    vec3 position;
    float roughness;
    float metallic;
    float ao;
    vec4 posMainLight;
};


struct SurfaceN
{
    vec3 position;
    vec4 posMainLight;
    vec3 vertexNormal;
    vec3 viewDir;
};

#endif