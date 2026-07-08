#pragma once
#include "../../Core/Config.h"
#include "../RenderTarget.h"

class RenderContext;
class Shader;
class Material;

class RenderPipeline
{
  public:
    virtual ~RenderPipeline() = default;

    virtual const char* GetName() const = 0;
    bool Init(const int width, const int height);
    void StartRender(RenderContext& context);

  protected:
    virtual bool OnInit(const int width, const int height)
    {
        return true;
    }
    virtual void Render(RenderContext& context) = 0;
    void DrawShadowMap(RenderContext& context);
    void DrawCustomDepth(RenderContext& context);
    void PostProcessing(RenderContext& context);
    virtual void DrawTransparent(RenderContext& context);

    void BindLightShadowMap();
    void UnbinLightShadowMap();
    void BindIBLTextures();

  private:
    void Bloom(RenderContext& context);

  protected:
    RenderTarget m_buf_shadow;
    RenderTarget m_bufLocShadow;
    RenderTarget m_buf_CustomDepth;
    RenderTarget m_bufOpaqueLight;
    RenderTarget m_bufTransparent;

    RenderTarget m_DwonSampleChain[5];
    RenderTarget m_BloomChainX[5];
    RenderTarget m_BloomChainY[5];

  private:
    std::shared_ptr<Material> m_shadowMaterialGloabl;
    std::shared_ptr<Material> m_shadowMaterialLocal;
};
