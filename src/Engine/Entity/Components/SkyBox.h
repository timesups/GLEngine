#pragma once
#include "Light.h"
#include <memory>

struct IBLImage;
class Mesh;
class Shader;

class SkyBox : public Light
{
  public:
    enum class CubemapPreview
    {
        PrefilteredMip0 = 0,
        Irradiance = 1,
    };

    SkyBox();
    ~SkyBox();

    void Init() override;
    void Update(float deltaTime) override;
    void Render() override;

    void SetIBL(std::shared_ptr<IBLImage>& ibl);
    std::shared_ptr<IBLImage>& GetIBL();

    void SetCubemapPreview(CubemapPreview preview);
    CubemapPreview GetCubemapPreview() const;

    /// 天空盒背景 cubemap 显示亮度（仅 SkyBox.glsl，不影响 PBR IBL）
    float GetBrightness() const;
    void SetBrightness(float brightness);

    /// PBR 漫反射 IBL（irradiance）是否参与 CalcAllLight
    bool IsIBLLightingEnabled() const;
    void SetIBLLightingEnabled(bool enabled);

    /// PBR IBL 强度倍率（irradiance × 此值）
    float GetIBLLightingIntensity() const;
    void SetIBLLightingIntensity(float intensity);

    /// 是否绘制天空盒背景（标定定向光时可关）
    bool IsDrawBackgroundEnabled() const;
    void SetDrawBackgroundEnabled(bool enabled);

  private:
    std::shared_ptr<IBLImage> m_ibl;
    std::unique_ptr<Mesh> m_cubeMesh;
    std::shared_ptr<Shader> m_skyShader;
    CubemapPreview m_cubemapPreview = CubemapPreview::PrefilteredMip0;
    float m_skyboxBrightness = 1.0f;
    bool m_iblLightingEnabled = true;
    float m_iblLightingIntensity = 1.0f;
    bool m_drawBackground = true;
};
