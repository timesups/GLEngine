#ifndef ENDFIELD_OUTLINE_INCLUDE
#define ENDFIELD_OUTLINE_INCLUDE
#include "Core.glsl"
#include "EndfieldLibrary.glsl"

uniform float _OutLineWidth;
#ifdef VERTEX
out vec2 uv;
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
    vec3 localPos = aPosition;
    localPos += N * _OutLineWidth;
    vec3 worldPos = ObjectToWorldPos(localPos);
    gl_Position = WorldToClipPos(worldPos);
    uv = aTexcoord0;
}
#endif

#ifdef FRAGMENT
uniform vec3 _OutLineColor;
uniform sampler2D _Mask;

in vec2 uv;
layout(location = 4) out vec4 outGBuffer4;
layout(location = 3) out vec4 outGBuffer3;

void main()
{

    vec4 mask = texture(_Mask,uv);
    if (mask.x < 0.3333)
        discard;


    outGBuffer4 = vec4(_OutLineColor,0.0);
    outGBuffer3 = vec4(_OutLineColor,0.0);
}
#endif

#endif