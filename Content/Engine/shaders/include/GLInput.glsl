#ifndef GLINPUT_INCLUDE
#define GLINPUT_INCLUDE
#extension GL_ARB_separate_shader_objects : enable//启动GL_ARB_separate_shader_objects
#extension GL_ARB_shading_language_420pack : enable//启动GL_ARB_shading_language_420pack
// MAXLOCALLIGHT 由 LoaderManager 根据 Config::MaxLocalLight 注入

//顶点数据输入
#ifdef VERTEX
    layout (location = 0) in vec3 aPosition;
    layout (location = 1) in vec3 aNormal;
    layout (location = 2) in vec3 aColor;
    layout (location = 3) in vec4 aTangent;
    layout (location = 4) in vec2 aTexcoord0;
    layout (location = 5) in vec2 aTexcoord1;
#endif

//相机相关数据输入
layout(std140,binding=0) uniform camera_buffer
{
    mat4 GL_MATRIX_V_NO_MOVE;
    mat4 GL_MATRIX_V;
    mat4 GL_I_MATRIX_V;
    mat4 GL_MATRIX_P;
    vec3 _CameraPosition;              
    float _zNear;                     
    float _zFar;    
    float _ScreenWidth;
    float _ScreenHeight;
    mat4 GL_I_MATRIX_P;
    float _time;
    float _fov;
    float _pad0;
    float _pad1;
    vec3 _CameraForward;
    float _pad;
};

//灯光数据输入
struct LocalLight
{
    vec3 position;  //0-12
    float inCutOff; //12-16
    vec3 direction; //16-28
    float outCutOff;//28-32
    vec3 color;     //32-44
    mat4 matrix[6];
    float zNear;
    float zFar;
    float bias;
    float pcfSample;
};

layout(std140,binding=1) uniform light_buffer
{
    vec3 _MainLightDirection;
    float _MainLightBias;
    vec3 _MainLightColor;
    float _MainLightPcfSample;
    vec3 _AmbientColor;
    float _LocalLightCount;
    mat4 _MainLightMatrix;
    float _MainLightZNear;
    float _MainLightZFar;
    float _IBLLightingEnabled;
    float _IBLLightingIntensity;
    float _MaxReflectionLOD;
    float _padIbl0;
    float _padIbl1;
    float _padIbl2;
    LocalLight _LocalLights[MAXLOCALLIGHT];
};
//后处理数据输入
layout(std140,binding=2) uniform post_processing_buffer
{
    float _exposure;
    float _gamma;
    float pad1;
    float pad2;
    //bloom
    float _bloom_threadshold;
    float _bloom_softkeen;
    float _bloom_intensity;
    float _bloom_enable;
    //ssao
    float _ssao_raduis;
    float _ssao_bias;
    float _ssao_pow;
    float _ssao_intensity;
    float _ao_mode;
    float _ao_pad0;
    float _ssao_pad1;
    float _ssao_pad2;

};

// GPU Instancing：与 C++ GPUInstanceData 布局一致
struct InstanceData
{
    mat4 model;
    mat4 normal;
    vec4 _InstanceBoundingBoxMax;
    vec4 _InstanceBoundingBoxMin;
};

layout(std430, binding = 3) buffer instance_buffer
{
    InstanceData _Instances[];
};

//需要逐物体输入的数据
uniform mat4 GL_MATRIX_M;
uniform mat4 GL_MATRIX_N;

uniform int _UseInstancing;
uniform int _InstanceOffset;
uniform int _InstanceCount;


uniform vec3 _BoundingBoxMax;
uniform vec3 _BoundingBoxMin;

//需要逐pass输入的数据
uniform int _PassIndex;
uniform int _DrawCount;

#endif
