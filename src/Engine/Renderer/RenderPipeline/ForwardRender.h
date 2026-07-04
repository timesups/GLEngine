#pragma once
#include "RenderPipeline.h"

class ForwardRender : public RenderPipeline
{
  public:
    const char* GetName() const override
    {
        return "Forward";
    }
    void Render(RenderContext& context) override;
};