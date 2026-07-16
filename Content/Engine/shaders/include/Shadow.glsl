#ifndef SHADOW_INCLUDE
#define SHADOW_INCLUDE






float CalcShadow(Light light,Surface s)
{

    if(light.isLoaclLight)
    {
        float currentDepth = distance(light.position,s.position);
        float bias = light.bias; 
        float samp = light.pcfSample;
        float offset = 0.01;

        
        float shadow = 0.0;
        for(float x = -offset; x < offset; x += offset / (samp * 0.5))
        {
            for(float y = -offset; y < offset; y += offset / (samp * 0.5))
            {
                for(float z = -offset; z < offset; z += offset / (samp * 0.5))
                {
                    vec3 sampleDir = normalize(-light.direction + vec3(x, y, z));
                    float closestDepth = texture(_LocalLightDepth, vec4(sampleDir, light.index)).r; 
                    closestDepth *= light.zFar;   // Undo mapping [0;1]
                    if(currentDepth - bias > closestDepth)
                        shadow += 1.0;
                }
            }
        }
        shadow /= (samp * samp * samp);

        return max(shadow,0.0);
    }
    else
    {
        //透视除法
        vec3 Proj_PositionLS = s.posMainLight.xyz/s.posMainLight.w;
        //to NDC
        Proj_PositionLS = Proj_PositionLS * 0.5 +0.5;
        float shadow = 0.0;
        vec2 size = 1.0/textureSize(_MainLightDepth,0);
        float currentDepth = Proj_PositionLS.z;

        int samp = int(light.pcfSample);

        for(int x=-samp;x<=samp;x++)
        {
            for(int y =-samp;y<=samp;y++)
            {
                float pcfDepth = texture(_MainLightDepth,Proj_PositionLS.xy + vec2(x,y) * size).x;
                shadow += currentDepth - light.bias > pcfDepth?1.0:0.0; 
            }
        }
        shadow /= pow((2*samp+1),2);
        if(currentDepth > 1.0)
            shadow = 0.0;
        return shadow;
    }


}





#endif