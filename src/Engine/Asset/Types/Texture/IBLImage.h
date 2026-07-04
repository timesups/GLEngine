#pragma once
#include <memory>
#include <string>

class TextureCube;

/// IBL 贴图集合：prefiltered（mip0=环境图，其余 mip 预留给镜面 prefilter）、irradiance（漫反射卷积）。
struct IBLImage
{
    std::string m_path;
    std::string m_name;
    bool m_showInUi = true;
    bool m_srgb = false;
    bool m_generateMips = false;
    int m_irradiancePhiSamples = 64;
    int m_irradianceThetaSamples = 32;
    int m_irradianceFaceSize = 32;
    /// <= 0 时根据等距柱状图尺寸自动推算
    int m_prefilterFaceSize = 0;
    int m_minPrefilterFaceSize = 512;

    std::shared_ptr<TextureCube> irradiance;
    /// mip 0 为环境 cubemap；更高 mip 供镜面 IBL prefilter 烘焙
    std::shared_ptr<TextureCube> prefiltered;
};
