#ifndef LIGHTINPUT_INCLUDE
#define LIGHTINPUT_INCLUDE

layout (binding=15) uniform samplerCube _irradianceMap;
layout (binding=14) uniform sampler2D _MainLightDepth;
layout (binding=13) uniform samplerCubeArray  _LocalLightDepth;
layout (binding=12) uniform samplerCube _prefiltered;
layout (binding=16) uniform sampler2D _brdfLUT;

#endif
