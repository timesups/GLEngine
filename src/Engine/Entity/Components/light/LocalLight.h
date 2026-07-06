#pragma once
#include "Light.h"

class LocalLight : public Light
{
  public:
    float GetShaderRadianceScale() const override;
    float GetIconBrightnessScale() const override;
    glm::mat4 GetProjectMatrix() const override;
    glm::mat4 GetViewMatrix(Direction direction = Direction::RIGHT) const override;

    float m_inCutoff = 180.f;
    float m_outCutoff = 180.f;
};
