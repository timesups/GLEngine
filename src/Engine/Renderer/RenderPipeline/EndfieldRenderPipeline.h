#pragma once

#include "memory"
#include "RenderPipeline.h"

class Shader;
class RenderTarget;

class EndfieldRenderPipeline : public RenderPipeline
{
  public:
    const char* GetName() const override
    {
        return "Endfield";
    }
    void Render(RenderContext& context) override;

  protected:
    bool OnInit(const int width, const int height) override;

  private:
    void LoadAsset();
    std::shared_ptr<Shader> m_defrredLightShader;
    std::shared_ptr<Shader> m_shadowShader;

    RenderTarget m_GBuffer;
    RenderTarget m_defrredLight;
    RenderTarget m_Shadow;
};
