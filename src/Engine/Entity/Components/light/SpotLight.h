#pragma once
#include "LocalLight.h"

class SpotLight : public LocalLight
{
  public:
    LightType GetType() const override
    {
        return LightType::SpotLight;
    }

    void Render() override;
};
