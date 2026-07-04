#ifndef TRANSFORM_INCLUDE
#define TRANSFORM_INCLUDE

mat4 GetModelMatrix()
{
#ifdef VERTEX
    if (_UseInstancing != 0)
        return _Instances[_InstanceOffset + gl_InstanceID].model;
#endif
    return GL_MATRIX_M;
}

mat4 GetNormalMatrix()
{
#ifdef VERTEX
    if (_UseInstancing != 0)
        return _Instances[_InstanceOffset + gl_InstanceID].normal;
#endif
    return GL_MATRIX_N;
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
