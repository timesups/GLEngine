#pragma once
#include "RenderPipeline.h"

#include <glm/glm.hpp>
#include <memory>
#include <vector>

class Shader;
class Material;
class Texture;

class EndfieldPipeline : public RenderPipeline
{
  public:
  EndfieldPipeline();
    ~EndfieldPipeline() override;
    const char* GetName() const override
    {
        return "EndfieldPipeline";
    }
    void Render(RenderContext& context) override;
  protected:
    bool OnInit(const int width, const int height) override;
    private:
    RenderTarget m_Gbuffer;
};
