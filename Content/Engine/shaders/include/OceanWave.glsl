#ifndef OCEAN_WAVE_INCLUDE
#define OCEAN_WAVE_INCLUDE

#define MAX_OCEAN_WAVES 16

struct OceanWave
{
    vec4 params;     // x=amplitude, y=wavelength, z=speed, w=steepness
    vec4 direction;  // xy=direction
};

layout(std140, binding = 4) uniform ocean_wave_buffer
{
    vec4 _OceanGlobals; // x=waveCount, y=gravity
    OceanWave _OceanWaves[MAX_OCEAN_WAVES];
};

#endif
