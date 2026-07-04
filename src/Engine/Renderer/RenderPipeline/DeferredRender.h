#pragma once
#include "RenderPipeline.h"

#include <glm/glm.hpp>
#include <memory>
#include <vector>

class Shader;
class Material;
class Texture;

class DeferredRender : public RenderPipeline
{
  public:
    DeferredRender();
    ~DeferredRender() override;
    const char* GetName() const override
    {
        return "Deferred";
    }
    void Render(RenderContext& context) override;

  protected:
    bool OnInit(const int width, const int height) override;

  private:
    void DrawGbuffer(RenderContext& context);
    void DrawSSAO(RenderContext& context);
    void DrawLighting(RenderContext& context);
    bool InitSSAO(const int width, const int height);

  private:
    std::shared_ptr<Shader> m_shaderDeferredLighting;
    RenderTarget m_buf_geo;
    RenderTarget m_buf_ssao;
    RenderTarget m_bufSSAOBlur;
    std::shared_ptr<Material> m_ssaoMaterial;
    std::vector<glm::vec3> m_ssaoKernel;
    std::unique_ptr<Texture> m_ssao_noise;
};
