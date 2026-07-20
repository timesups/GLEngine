#ifndef ENDFILED_LIBRARY_INCLUDE
#define ENDFILED_LIBRARY_INCLUDE
#define Q_SCALE 0.002

float unpackSnorm10(uint bits)
{
    float q = float(bits);
    return (q >= 512.0) ? (q - 1024.0) : q;
}

vec3 decodeOctahedral(vec2 oct)
{
    float z = 1.0 - abs(oct.x) - abs(oct.y);
    vec2 xy = (z < 0.0) ? (vec2(1.0) - abs(oct.yx)) * sign(oct) : oct;
    return normalize(vec3(xy, z));
}

vec3 buildOrthogonalTangent(vec3 n)
{
    vec3 a = n.yzx - n.zxy;
    return normalize(a - n * dot(a, n));
}

void unpackNormalTangent(uint packedNormal, out vec3 N, out vec4 T)
{
    uint qxBits = (packedNormal >> 0) & 0x3FFu;
    uint qyBits = (packedNormal >> 10) & 0x3FFu;
    uint qzBits = (packedNormal >> 20) & 0x3FFu;
    uint wBit = (packedNormal >> 31) & 0x1u;

    vec2 oct = vec2(unpackSnorm10(qxBits), unpackSnorm10(qyBits)) * Q_SCALE;
    N = decodeOctahedral(oct);

    vec3 T0 = buildOrthogonalTangent(N);
    vec3 B0 = normalize(cross(N, T0));

    float param = unpackSnorm10(qzBits) * Q_SCALE;
    float s = (param < 0.0) ? -1.0 : 1.0;
    float u = 1.0 - 2.0 * abs(param);
    vec2 tb = normalize(vec2(u, s * (1.0 - abs(u))));

    T = vec4(normalize(T0 * tb.x + B0 * tb.y), float(wBit) * 2.0 - 1.0);
}

vec3 unpackNormalMapWY(sampler2D normalMap, vec2 uv, float scale)
{
    vec4 t = texture(normalMap, uv);
    t.w = t.w * t.x;

    vec2 wy = t.wy;
    vec2 oct = wy * 2.0 - 1.0;
    float z = sqrt(max(0.0, 1.0 - dot(oct, oct)));
    return vec3(oct.x * scale, oct.y * scale, z);
}

vec2 encodeNormalEndfield(vec3 n)
{
    n = normalize(n);
    float sum = dot(abs(n), vec3(1.0));
    vec2 xz = n.xz / sum;

    if (n.y <= 0.0)
        xz = (vec2(1.0) - abs(xz.yx)) * sign(xz);

    return xz * 0.5 + 0.5;
}

vec4 endfieldApplyClipScale(vec4 clipPos, vec4 clipScale)
{
    vec2 scaled = clipScale.zw * vec2(2.0, -2.0) * clipPos.w;
    vec2 xy = clipPos.xy - scaled;
    return vec4(xy.x, -xy.y, clipPos.z, clipPos.w);
}

#endif
