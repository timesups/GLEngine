#pragma once
#include "LocalLight.h"

class PointLight : public LocalLight
{
  public:
    LightType GetType() const override
    {
        return LightType::PointLight;
    }

    void Render() override;
};
