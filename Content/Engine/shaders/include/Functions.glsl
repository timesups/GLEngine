#ifndef FUNCTIONS_INCLUDE
#define FUNCTIONS_INCLUDE




float LinearizeDepth(float depth) 
{
    float z = depth * 2.0 - 1.0; // 转换为 NDC
    return (2.0 * _zNear * _zFar) / (_zFar + _zNear - z * (_zFar - _zNear));    
}


vec2 EncodeNormalOct(vec3 n)
{
    n = normalize(n);
    // 投影到八面体
    vec2 p = n.xy * (1.0 / (abs(n.x) + abs(n.y) + abs(n.z)));
    // 负 Z 半球折叠
    if (n.z < 0.0)
        p = (1.0 - abs(p.yx)) * sign(p);
    // 映射到 [0,1]
    return p * 0.5 + 0.5;
}
vec3 DecodeNormalOct(vec2 enc)
{
    vec2 p = enc * 2.0 - 1.0;
    vec3 n = vec3(p.x, p.y, 1.0 - abs(p.x) - abs(p.y));
    // 负 Z 半球展开
    if (n.z < 0.0)
        n.xy = (1.0 - abs(n.yx)) * sign(n.xy);
    return normalize(n);
}

// 终末地 GBuffer 法线编码（Y 轴折叠，与 Gbuffer_Endfield 对应）
vec3 DecodeNormalEndfield(vec2 enc)
{
    vec2 xz = enc * 2.0 - 1.0;
    float y = 1.0 - abs(xz.x) - abs(xz.y);

    if (y <= 0.0)
    {
        xz = (vec2(1.0) - abs(xz.yx)) * sign(xz);
        y = 1.0 - abs(xz.x) - abs(xz.y);
    }

    return normalize(vec3(xz.x, y, xz.y));
}

// bent normal 编码逆运算（NormalXY.zw）
vec2 DecodeBentNormal(vec2 enc)
{
    vec2 q = enc * 2.0 - 1.0;
    vec2 s = sign(q);
    vec2 b = abs(q);
    vec2 v = s * b * b * b * b;
    return v * 2.0;
}

vec3 ReconstructPositionWS(vec2 uv, float depth)
{
    vec4 clipPos = vec4(uv * 2.0 - 1.0, depth * 2.0 - 1.0, 1.0);
    vec4 viewPos = GL_I_MATRIX_P * clipPos;
    viewPos /= viewPos.w;
    return (GL_I_MATRIX_V * viewPos).xyz;
}
vec3 DeriveNormalZ(vec2 xy)
{
    float x = xy.x;
    float y = xy.y;
    float z = sqrt(1-(x*x+y*y));
    return normalize(vec3(x,y,z));
}

#ifdef FRAGMENT
    vec3 UnpackNormal(sampler2D normal,vec2 uv,bool NoZ=false)
    {
       
        vec3 normalTS = texture(normal, uv).xyz * 2.0 - 1.0;
        if(NoZ)
        {
            return DeriveNormalZ(normalTS.xy);
        }
        else
        {
            return normalTS;
        }
    }

    float GetPixelDepth()
    {
        return LinearizeDepth(gl_FragCoord.z);
    }
#endif






#endif