#ifndef TRANSFORM_INCLUDE
#define TRANSFORM_INCLUDE

// 顶点写入 instance 索引，片段通过 flat varying 读取（当前 profile 片段不可用 gl_InstanceID）
#ifdef VERTEX
flat out int v_InstanceIndex;
#elif defined(FRAGMENT)
flat in int v_InstanceIndex;
#endif

int GetInstanceIndex()
{
#ifdef VERTEX
    v_InstanceIndex = (_UseInstancing != 0) ? (_InstanceOffset + gl_InstanceID) : 0;
    return v_InstanceIndex;
#elif defined(FRAGMENT)
    return v_InstanceIndex;
#else
    return 0;
#endif
}

mat4 GetModelMatrix()
{
    if (_UseInstancing != 0)
        return _Instances[GetInstanceIndex()].model;
    return GL_MATRIX_M;
}

mat4 GetNormalMatrix()
{
    if (_UseInstancing != 0)
        return _Instances[GetInstanceIndex()].normal;
    return GL_MATRIX_N;
}

vec3 GetBoundingBoxMax()
{
    if (_UseInstancing != 0)
        return _Instances[GetInstanceIndex()]._InstanceBoundingBoxMax.xyz;
    return _BoundingBoxMax;
}

vec3 GetBoundingBoxMin()
{
    if (_UseInstancing != 0)
        return _Instances[GetInstanceIndex()]._InstanceBoundingBoxMin.xyz;
    return _BoundingBoxMin;
}

vec4 ObjectToClipPos(vec3 pos)
{
    return GL_MATRIX_P * GL_MATRIX_V * GetModelMatrix() * vec4(pos, 1.0);
}

vec4 WorldToClipPos(vec3 pos)
{
    return GL_MATRIX_P * GL_MATRIX_V * vec4(pos, 1.0);
}

vec3 ObjectToWorldN(vec3 normal)
{
    return normalize(mat3(GetNormalMatrix()) * normal);
}

vec3 ObjectToWorldDir(vec3 dir)
{
    return normalize(GetModelMatrix() * vec4(dir, 0.0)).xyz;
}

vec3 ObjectToWorldPos(vec3 pos)
{
    return (GetModelMatrix() * vec4(pos, 1.0)).xyz;
}

#endif
