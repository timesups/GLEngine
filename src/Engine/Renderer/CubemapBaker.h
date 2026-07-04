#pragma once

#include <memory>
class Texture2D;
class TextureCube;
class Texture;
class Shader;

struct EquirectToCubemapParams
{
    /// prefiltered mip0 每面边长；<= 0 时根据等距柱状图尺寸自动推算
    int faceSize = 0;
    /// irradiance cubemap 每面边长；<= 0 时使用默认 32
    int irradianceFaceSize = 32;
    /// irradiance 卷积水平采样数（phi）
    int irradiancePhiSamples = 64;
    /// irradiance 卷积垂直采样数（theta）
    int irradianceThetaSamples = 32;
    bool srgb = false;
    /// 为 prefiltered 预分配完整 mip 链（镜面 IBL 需要；false 时仅 mip0）
    bool generateMips = true;
    /// prefiltered cubemap mip0 最小边长（<=0 时不强制）
    int minPrefilterFaceSize = 512;
};

class CubemapBaker
{
  public:
    static int InferFaceSizeFromEquirect(int equirectWidth, int equirectHeight);

    /// equirect → prefiltered（含镜面 prefilter mip 链） + irradiance 卷积
    static bool BakeIBLFromEquirect(Texture2D& equirect, TextureCube& outPrefiltered, TextureCube& outIrradiance,
                                    const EquirectToCubemapParams& params);

    /// 全局 BRDF 积分 LUT（引擎启动时烘焙一次，binding=16）
    static std::shared_ptr<Texture2D> EnsureBrdfLut();
    static void InvalidateBrdfLut();

  private:
    static bool BakeCubemapFaces(Texture* texSource, TextureCube* texDes, const std::shared_ptr<Shader>& shader,
                                 const char* sourceSamplerUniform, int destMipLevel = 0, int irradiancePhiSamples = 0,
                                 int irradianceThetaSamples = 0, float roughness = -1.0f, int cubeMapResolution = 0);
    static void SetupCubemapMipFiltering(TextureCube& cube);
    static bool BakeBrdfLUT(Texture2D& outLut);
};
