#pragma once
#include "RenderPipeline.h"

#include "../AmbientOcclusion.h"
#include <glm/glm.hpp>
#include <memory>
#include <vector>

class Shader;
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
    void DrawAmbientOcclusion(const RenderContext& context);
    void DrawSsao(const RenderContext& context);
    void DrawHbao(const RenderContext& context);
    void DrawLighting(RenderContext& context);
    void DrawTransparent(RenderContext& context) override;
    bool InitSsao(const int width, const int height);
    void UploadSsaoKernel(const Shader& shader) const;
    bool HasAoResources() const;

  private:
    std::shared_ptr<Shader> m_shaderDeferredLighting;
    std::shared_ptr<Shader> m_shaderSsao;
    std::shared_ptr<Shader> m_shaderSsaoBlur;
    RenderTarget m_Gbuffer;
    RenderTarget m_buf_ao;
    RenderTarget m_buf_aoBlur;
    std::vector<glm::vec3> m_ssaoKernel;
    std::unique_ptr<Texture> m_ssao_noise;
    bool m_ssaoKernelUploaded = false;
};
